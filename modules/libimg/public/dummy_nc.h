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

/* This is a dummy Net Context which the Image Library uses for network
   operations in lieu of an MWContext.  It will be replaced by a true
   Net Context when the Network Library is modularized. */

#ifndef _DUMMY_NC_H
#define _DUMMY_NC_H

#include "prtypes.h"

typedef void* IL_NetContext;

PR_BEGIN_EXTERN_C

extern IL_NetContext *
IL_NewDummyNetContext(MWContext *context,
                      NET_ReloadMethod cache_reload_policy);

extern void
IL_DestroyDummyNetContext(IL_NetContext *net_cx);

PR_END_EXTERN_C

#endif /* _DUMMY_NC_H */
