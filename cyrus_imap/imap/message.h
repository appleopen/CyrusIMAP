/* message.h -- Message parsing
 $Id: message.h,v 1.6 2003/02/13 20:15:28 rjs3 Exp $

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
#ifndef INCLUDED_MESSAGE_H
#define INCLUDED_MESSAGE_H

#ifndef P
#ifdef __STDC__
#define P(x) x
#else
#define P(x) ()
#endif
#endif

#include <stdio.h>

#include "prot.h"
#include "mailbox.h"

extern int message_copy_strict P((struct protstream *from, FILE *to,
				  unsigned size));

/* Flags for parsing message date/time - to be bitwise OR'd */
#define PARSE_DATE	(1<<0)
#define PARSE_TIME	(1<<1)
#define PARSE_ZONE	(1<<2)

extern time_t message_parse_date P((char *hdr, unsigned flags));
extern int message_parse_file P((FILE *infile, struct mailbox *mailbox,
				 struct index_record *message_index));
extern int message_parse_mapped P((const char *msg_base, unsigned long msg_len,
				   struct mailbox *mailbox,
				   struct index_record *message_index));

#endif /* INCLUDED_MESSAGE_H */
