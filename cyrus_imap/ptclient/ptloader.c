/* ptloader.c -- group loader daemon
 */
/*
 * Copyright (c) 1998-2003 Carnegie Mellon University.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any other legal
 *    details, please contact  
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <config.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>

#include "auth_pts.h"
#include "cyrusdb.h"
#include "exitcodes.h"
#include "hash.h"
#include "global.h"
#include "libconfig.h"
#include "lock.h"
#include "retry.h"
#include "xmalloc.h"
#include "ptloader.h"

static char rcsid[] __attribute__((unused)) = 
      "$Id: ptloader.c,v 1.44 2007/01/09 21:25:04 jeaton Exp $";

struct pts_module *pts_modules[] = {
#ifdef HAVE_LDAP
    &pts_ldap,
#endif
#ifdef HAVE_AFSKRB
    &pts_afskrb,
#endif
    NULL };

extern void setproctitle_init(int argc, char **argv, char **envp);

static struct pts_module *pts_fromname()
{
    int i;
    const char *name = config_getstring(IMAPOPT_PTS_MODULE);
    static struct pts_module *pts = NULL;
    
    if (pts)
        return pts;
    
    for (i = 0; pts_modules[i]; i++) {
	if (!strcmp(pts_modules[i]->name, name)) {
	    pts = pts_modules[i]; break;
	}
    }

    if (!pts) {
	char errbuf[1024];
	snprintf(errbuf, sizeof(errbuf),
		 "PTS module %s not supported", name);
	fatal(errbuf, EC_CONFIG);
    }
    
    return pts;
}

void ptsmodule_init(void)
{
    struct pts_module *pts = pts_fromname();
    
    pts->init();
}

struct auth_state *ptsmodule_make_authstate(const char *identifier,
					    size_t size,
					    const char **reply, int *dsize)
{
    struct pts_module *pts = pts_fromname();
    
    return pts->make_authstate(identifier, size, reply, dsize);
}

/* config.c info (libimap) */
const int config_need_data = 0;

/* Globals */
#define DB (config_ptscache_db)

/* XXXXXXXXX */
void des_init_random_number_generator() {
        return;
} 

static char ptclient_debug = 0;
struct db *ptsdb = NULL;
  
int service_init(int argc, char *argv[], char **envp __attribute__((unused)))
{
    int r;
    int opt;
    char fnamebuf[1024];
    extern char *optarg;

    if (geteuid() == 0) fatal("must run as the Cyrus user", EC_USAGE);
    setproctitle_init(argc, argv, envp);

    /* set signal handlers */
    signal(SIGPIPE, SIG_IGN);

    syslog(LOG_NOTICE, "starting: $Id: ptloader.c,v 1.44 2007/01/09 21:25:04 jeaton Exp $");

    while ((opt = getopt(argc, argv, "d:")) != EOF) {
	switch (opt) {
	case 'd':
	    ptclient_debug = atoi(optarg);
	    if (ptclient_debug < 1) {
		ptclient_debug = 1;
	    }
	    break;
	default:
            syslog(LOG_ERR, "invalid command line option specified");
	    break;
	    /* just pass through */
	}
    }

    strcpy(fnamebuf, config_dir);
    strcat(fnamebuf, PTS_DBFIL);
    r = DB->open(fnamebuf, CYRUSDB_CREATE, &ptsdb);
    if (r != 0) {
	syslog(LOG_ERR, "DBERROR: opening %s: %s", fnamebuf,
	       cyrusdb_strerror(ret));
	fatal("can't read pts database", EC_TEMPFAIL);
    }

    ptsmodule_init();

    return 0;
}

/* Called by service API to shut down the service */
void service_abort(int error)
{
    int r;

    r = DB->close(ptsdb);
    if (r) {
	syslog(LOG_ERR, "DBERROR: error closing ptsdb: %s",
	       cyrusdb_strerror(r));
    }

    r = DB->done();
    if (r) {
	syslog(LOG_ERR, "DBERROR: error exiting application: %s",
	       cyrusdb_strerror(r));
    }

    exit(error);
}

/* we're a 'threaded' service, but since we never fork or create any
   threads, we're just one-person-at-a-time based */
int service_main_fd(int c, int argc __attribute__((unused)),
		    char **argv __attribute__((unused)),
		    char **envp __attribute__((unused)))
{
    char keyinhex[512];
    const char *reply = NULL;
    char user[PTS_DB_KEYSIZE];
    int rc, dsize;
    size_t size;
    struct auth_state *newstate;

    (void)memset(&size, 0, sizeof(size));
    if (read(c, &size, sizeof(size_t)) < 0) {
        syslog(LOG_ERR, "socket (size): %m");
        reply = "Error reading request (size)";
        goto sendreply;
    }

    if (size > PTS_DB_KEYSIZE)  {
	syslog(LOG_ERR, "size sent %d is greater than buffer size %d", 
	       size, PTS_DB_KEYSIZE);
	reply = "Error: invalid request size";
	goto sendreply;
    }

    if (size == 0) {
        syslog(LOG_ERR, "size sent is 0");
        reply = "Error: zero request size";
        goto sendreply;
    }

    memset(&user, 0, sizeof(user));
    if (read(c, &user, size) < 0) {
        syslog(LOG_ERR, "socket(user; size = %d; key = %s): %m", 
	       size, keyinhex);
        reply = "Error reading request (user)";
        goto sendreply;
    }

    if (ptclient_debug) {
	syslog(LOG_DEBUG, "user %s, cacheid %s", user, keyinhex);
    }


    newstate = ptsmodule_make_authstate(user, size, &reply, &dsize);

    if(newstate) {
	/* Success! */
	rc = DB->store(ptsdb, user, size, (void *)newstate, dsize, NULL);
        free(newstate);
	
	/* and we're done */
	reply = "OK";
    }

 sendreply:
    if (retry_write(c, reply, strlen(reply)) <0) {
	syslog(LOG_WARNING, "retry_write: %m");
    }
    close(c);

    return 0;
}

/* we need to have this function here 'cause libcyrus.a 
 * makes calls to this function. 
 */
void fatal(const char *msg, int exitcode)
{
    syslog(LOG_ERR, "%s", msg);
    exit(exitcode);
}

void printstring(const char *s __attribute__((unused)))
{
    /* needed to link against annotate.o */
    fatal("printstring() executed, but its not used for PTLOADER!",
	  EC_SOFTWARE);
}
