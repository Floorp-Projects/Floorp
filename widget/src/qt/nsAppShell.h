/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsAppShell_h__
#define nsAppShell_h__

#include "nsIAppShell.h"
#include "nsIEventQueue.h"

#include "nsQApplication.h"
#include <qwindowsstyle.h>

/**
 * Native QT Application shell wrapper
 */

class nsAppShell : public nsIAppShell
{
public:
    nsAppShell();
    virtual ~nsAppShell();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIAPPSHELL

private:
    nsDispatchListener	    *mDispatchListener;
    nsCOMPtr<nsIEventQueue> mEventQueue;
    nsQApplication          *mApplication;
    static PRBool           mRunning;
};

#endif // nsAppShell_h__

