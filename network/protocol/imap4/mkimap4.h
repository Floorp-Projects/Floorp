/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef MKIMAP4_H
#define MKIMAP4_H

#include "mkgeturl.h"

XP_BEGIN_PROTOS

extern int32 NET_IMAP4Load (ActiveEntry *ce);

/* NET_ProcessIMAP4  will control the state machine that
 * loads messages from a imap4 server
 *
 * returns negative if the transfer is finished or error'd out
 *
 * returns zero or more if the transfer needs to be continued.
 */
extern int32 NET_ProcessIMAP4 (ActiveEntry *ce);

/* abort the connection in progress
 */
MODULE_PRIVATE int32 NET_InterruptIMAP4(ActiveEntry * ce);

extern void NET_InitIMAP4Protocol(void);

XP_END_PROTOS


#endif /* MKIMAP4_H */
