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

// 
// nsAppShell
//
// This file contains the default interface of the application shell. Clients
// may either use this implementation or write their own. If you write your
// own, you must create a message sink to route events to. (The message sink
// interface may change, so this comment must be updated accordingly.)
//

#ifndef nsAppShell_h__
#define nsAppShell_h__

#include "nsIAppShell.h"

#include <memory>

class nsMacMessagePump;
class nsMacMessageSink;
class nsToolkit;


class nsAppShell : public nsIAppShell
{
  private:
    nsDispatchListener             *mDispatchListener;    // note: we don't own this, but it can be NULL
    auto_ptr<nsToolkit>            mToolKit;
    auto_ptr<nsMacMessagePump>     mMacPump;
    nsMacMessageSink               *mMacSink;             //еее this will be COM, so use scc's COM_auto_ptr
    PRBool                         mExitCalled;
		static PRBool									mInitializedToolbox;

	// CLASS METHODS
	private:		    
		    
		    
  public:
    nsAppShell();
    virtual ~nsAppShell();

    NS_DECL_ISUPPORTS

    // nsIAppShellInterface
  
    NS_IMETHOD              Create(int* argc, char ** argv);
    virtual nsresult        Run();
    NS_IMETHOD              Spinup();
    NS_IMETHOD              Spindown();
    NS_IMETHOD              Exit();
    NS_IMETHOD            	SetDispatchListener(nsDispatchListener* aDispatchListener);

    virtual void* GetNativeData(PRUint32 aDataType);

    NS_IMETHOD GetNativeEvent(PRBool &aRealEvent, void *&aEvent);
    NS_IMETHOD DispatchNativeEvent(PRBool aRealEvent, void *aEvent);
	NS_IMETHOD EventIsForModalWindow(PRBool aRealEvent, void *aEvent, nsIWidget *aWidget,
                                  PRBool *aForWindow);
};

#endif // nsAppShell_h__

