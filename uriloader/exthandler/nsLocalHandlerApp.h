/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:expandtab:shiftwidth=2:tabstop=2:cin:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsLocalHandlerAppImpl_h__
#define __nsLocalHandlerAppImpl_h__

#include "nsString.h"
#include "nsIMIMEInfo.h"
#include "nsIFile.h"
#include "nsTArray.h"

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
  nsString mDetailedDescription;
  nsTArray<nsString> mParameters;
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
