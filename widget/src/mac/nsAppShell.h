/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsIToolkit.h"

#include <memory>

using std::auto_ptr;

class nsMacMessagePump;
class nsMacMessageSink;
class nsMacMemoryCushion;


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
    auto_ptr<nsMacMessageSink>     mMacSink;             // this will be COM, so use scc's COM_auto_ptr
    auto_ptr<nsMacMemoryCushion>    mMacMemoryCushion;
    PRBool                         mExitCalled;
	static PRBool                  mInitializedToolbox;
};

#endif // nsAppShell_h__

