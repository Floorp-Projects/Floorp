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

#ifndef SOCKSTUB_H
#define SOCKSTUB_H

#include "prio.h"

/* forward declaration */
struct URL_Struct_;


extern "C" void
NET_InitSockStubProtocol(void);

/* XXX: Hack the following is a realy hack. Should go away */
extern "C" PRFileDesc *
NET_GetSocketToHashTable(URL_Struct_* URL_s);

extern "C" int32
NET_FreeSocket(URL_Struct_* URL_s);

#endif /* SOCKSTUB_H */
