/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef SWITCHTOUITHREAD_H
#define SWITCHTOUITHREAD_H

#include "nsISupports.h"

// foreward declaration
struct MethodInfo;

/**
 * Switch thread to match the thread the widget was created in so messages will be dispatched.
 */

class nsSwitchToUIThread {

public:
    virtual bool CallMethod(MethodInfo *info) = 0;

};

//
// Structure used for passing the information necessary for synchronously 
// invoking a method on the GUI thread...
//
struct MethodInfo {
	nsISupports			*widget;
    nsSwitchToUIThread	*target;
    uint32				methodId;
    int					nArgs;
    uint32				*args;

    MethodInfo(nsISupports *ref, nsSwitchToUIThread *obj, uint32 id, int numArgs = 0, uint32 *arguments = 0)
    {
		widget = ref;
		NS_ADDREF(ref);
		target   = obj;
		methodId = id;
		nArgs    = numArgs;
		args = new uint32 [numArgs];
		memcpy(args, arguments, sizeof(uint32) * numArgs);
    }

	~MethodInfo()
	{
		delete [] args;
		NS_RELEASE(widget);
	}

    bool Invoke() { return target->CallMethod(this); }
};

#endif // TOUITHRD_H

