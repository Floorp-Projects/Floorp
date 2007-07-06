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

#ifndef __nshandlerappimpl_h__
#define __nshandlerappimpl_h__

#include "nsString.h"
#include "nsIMIMEInfo.h"
#include "nsIFile.h"

class nsHandlerAppBase : public nsIHandlerApp
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHANDLERAPP

  nsHandlerAppBase() NS_HIDDEN {}
  nsHandlerAppBase(const PRUnichar *aName) NS_HIDDEN  { mName.Assign(aName); }
  nsHandlerAppBase(const nsAString & aName) NS_HIDDEN  { mName.Assign(aName); }
  virtual ~nsHandlerAppBase() {}

protected:
  nsString mName;
};

class nsLocalHandlerApp : public nsHandlerAppBase, public nsILocalHandlerApp
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSILOCALHANDLERAPP
  
  nsLocalHandlerApp() {}

  nsLocalHandlerApp(const PRUnichar *aName, nsIFile *aExecutable) 
    : nsHandlerAppBase(aName), mExecutable(aExecutable) {}

  nsLocalHandlerApp(const nsAString & aName, nsIFile *aExecutable) 
    : nsHandlerAppBase(aName), mExecutable(aExecutable) {}

  virtual ~nsLocalHandlerApp() {}

  // overriding to keep old caching behavior (that a useful name is returned
  // even if none was given to the constructor)
  NS_IMETHOD GetName(nsAString & aName);
    
protected: 
  nsCOMPtr<nsIFile> mExecutable;
};

class nsWebHandlerApp : public nsHandlerAppBase, public nsIWebHandlerApp
{
  public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIWEBHANDLERAPP

  nsWebHandlerApp(const PRUnichar *aName, const nsACString &aUriTemplate)
    : nsHandlerAppBase(aName), mUriTemplate(aUriTemplate) { }

  virtual ~nsWebHandlerApp() {}

  protected:
  nsCString mUriTemplate;
      
};

#endif //  __nshandlerappimpl_h__
