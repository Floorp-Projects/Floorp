/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
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

#ifndef nsAppShell_h__
#define nsAppShell_h__

#include "nsObject.h"
#include "nsIAppShell.h"

/**
 * Native Win32 Application shell wrapper
 */

class nsAppShell : public nsIAppShell, public nsObject
{
  public:
                            nsAppShell(nsISupports *aOuter); 
    virtual                 ~nsAppShell();

    // nsISupports. Forward to the nsObject base class
    BASE_SUPPORT

    virtual nsresult        QueryObject(const nsIID& aIID, void** aInstancePtr);
    PRBool                  OnPaint();

    // nsIAppShellInterface
  
    virtual void            Create();
    virtual nsresult        Run(); 
    virtual void            SetDispatchListener(nsDispatchListener* aDispatchListener);
    virtual void            Exit();
  private:
    nsDispatchListener*     mDispatchListener;
};

#endif // nsAppShell_h__
