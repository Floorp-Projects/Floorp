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

#ifndef _NS_SOFTWAREUPDATEQUEUE_H__
#define _NS_SOFTWAREUPDATEQUEUE_H__


#include "ntypes.h"        /* for MWContext */

typedef void (*SoftUpdateCompletionFunction) (int result, void * closure);

/* a struct to hold the arguments */
struct su_startCallback_
{
    char * url;
    char * name;
    SoftUpdateCompletionFunction f;
    void * completionClosure;
    int32 flags;
    MWContext * context;

};

typedef su_startCallback_ su_startCallback;


NSPR_BEGIN_EXTERN_C

PRBool QInsert( su_startCallback * Item );
su_startCallback *QGetItem(void);

NSPR_END_EXTERN_C

#endif


