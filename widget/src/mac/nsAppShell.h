/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
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
#include "nsCOMPtr.h"

#include <memory>

class nsMacMessagePump;
class nsMacMessageSink;
class nsIToolkit;


class nsAppShell : public nsIAppShell
{
  public:
    nsAppShell();
    virtual ~nsAppShell();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIAPPSHELL
  
  private:
    nsDispatchListener             *mDispatchListener;    // note: we don't own this, but it can be NULL
    nsCOMPtr<nsIToolkit>           mToolkit;
    auto_ptr<nsMacMessagePump>     mMacPump;
    auto_ptr<nsMacMessageSink>     mMacSink;             //€€€ this will be COM, so use scc's COM_auto_ptr
    PRBool                         mExitCalled;
	static PRBool                  mInitializedToolbox;
};

#endif // nsAppShell_h__

