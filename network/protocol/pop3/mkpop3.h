/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef MKPOP3_H
#define MKPOP3_H

extern void NET_InitPop3Protocol(void);

MODULE_PRIVATE XP_Bool NET_GetAllowAtSignInMailUserName();
MODULE_PRIVATE int32 net_ProcessPop3 (ActiveEntry *ce);
MODULE_PRIVATE void NET_SetAllowAtSignInMailUserName(XP_Bool allow);

#endif /* not MKPOP3_H */
