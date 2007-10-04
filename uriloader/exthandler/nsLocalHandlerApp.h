/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:expandtab:shiftwidth=2:tabstop=2:cin:
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is 
 * the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dan Mosedale <dmose@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __nsLocalHandlerAppImpl_h__
#define __nsLocalHandlerAppImpl_h__

#include "nsString.h"
#include "nsIMIMEInfo.h"
#include "nsIFile.h"

class nsLocalHandlerApp : public nsILocalHandlerApp
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHANDLERAPP
  NS_DECL_NSILOCALHANDLERAPP

  nsLocalHandlerApp() { }

  nsLocalHandlerApp(const PRUnichar *aName, nsIFile *aExecutable) 
    : mName(aName), mExecutable(aExecutable) { }

  nsLocalHandlerApp(const nsAString & aName, nsIFile *aExecutable) 
    : mName(aName), mExecutable(aExecutable) { }
  virtual ~nsLocalHandlerApp() { }

protected:
  nsString mName;
  nsCOMPtr<nsIFile> mExecutable;
  
  /**
   * Launches this application with a single argument (typically either
   * a file path or a URI spec).  This is meant as a helper method for
   * implementations of (e.g.) LaunchWithURI.
   *
   * @param aApp The application to launch (may not be null)
   * @param aArg The argument to pass on the command line
   */
   NS_HIDDEN_(nsresult) LaunchWithIProcess(const nsCString &aArg);

};

// any platforms that need a platform-specific class instead of just 
// using nsLocalHandlerApp need to add an include and a typedef here.
#ifdef XP_MACOSX
# ifndef NSLOCALHANDLERAPPMAC_H_  
# include "mac/nsLocalHandlerAppMac.h"
typedef nsLocalHandlerAppMac PlatformLocalHandlerApp_t;
# endif
#else 
typedef nsLocalHandlerApp PlatformLocalHandlerApp_t;
#endif

#endif //  __nsLocalHandlerAppImpl_h__
