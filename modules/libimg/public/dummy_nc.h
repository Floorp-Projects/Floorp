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

/* This is a dummy Net Context which the Image Library uses for network
   operations in lieu of an MWContext.  It will be replaced by a true
   Net Context when the Network Library is modularized. */

#ifndef _DUMMY_NC_H
#define _DUMMY_NC_H

#include "xp_mem.h"
#include "net.h"

typedef struct {
    void *cx;                             /* Opaque pointer to an MWContext. */
    NET_ReloadMethod cache_reload_policy;
} IL_NetContext;

typedef URL_Struct IL_URL;
typedef NET_StreamClass IL_Stream;

XP_BEGIN_PROTOS
extern IL_NetContext *
IL_NewDummyNetContext(MWContext *context,
                      NET_ReloadMethod cache_reload_policy);

extern void
IL_DestroyDummyNetContext(IL_NetContext *net_cx);

extern IL_NetContext *
IL_CloneDummyNetContext(IL_NetContext *net_cx);

extern void
IL_AddReferer(IL_NetContext *net_cx, URL_Struct *urls);
XP_END_PROTOS

#endif /* _DUMMY_NC_H */
