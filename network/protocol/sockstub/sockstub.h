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

#ifndef SOCKSTUB_H
#define SOCKSTUB_H

#include "prio.h"

/* forward declaration */
struct URL_Struct_;


extern "C" void
NET_InitSockStubProtocol(void);

/* XXX: Hack the following is a realy hack. Should go away */
PUBLIC PRFileDesc *
NET_GetSocketToHashTable(URL_Struct_* URL_s);

PUBLIC int32
NET_FreeSocket(URL_Struct_* URL_s);

#endif /* SOCKSTUB_H */
