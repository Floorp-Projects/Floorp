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


// foreward declaration
struct MethodInfo;

class nsSwitchToUIThread {

public:
    virtual BOOL CallMethod(MethodInfo *info) = 0;

};

//
// Structure used for passing the information necessary for synchronously 
// invoking a method on the GUI thread...
//
struct MethodInfo {
    nsSwitchToUIThread* target;
    UINT        methodId;
    int         nArgs;
    DWORD*      args;

    MethodInfo(nsSwitchToUIThread *obj, UINT id, int numArgs=0, DWORD *arguments = 0) {
        target   = obj;
        methodId = id;
        nArgs    = numArgs;
        args     = arguments;
    }
    
    BOOL Invoke() { return target->CallMethod(this); }
};

#endif // TOUITHRD_H

