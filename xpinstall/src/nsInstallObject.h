/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#ifndef nsInstallObject_h__
#define nsInstallObject_h__

#include "prtypes.h"

class nsInstall;

class nsInstallObject 
{
    public:
        /* Public Methods */
        nsInstallObject(nsInstall* inInstall) {mInstall = inInstall; }

        /* Override with your set-up action */
        virtual PRInt32 Prepare() = 0;

        /* Override with your Completion action */
        virtual PRInt32 Complete() = 0;

        /* Override with an explanatory string for the progress dialog */
        virtual PRUnichar* toString() = 0;

        /* Override with your clean-up function */
        virtual void Abort() = 0;

        /* should these be protected? */
        virtual PRBool CanUninstall() = 0;
        virtual PRBool RegisterPackageNode() = 0;

    protected:
        nsInstall* mInstall;
};

#endif /* nsInstallObject_h__ */
