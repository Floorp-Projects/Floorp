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

#include "nscore.h"

#ifdef NS_WIN32
#include "windows.h"
#endif

#include "nsApplicationManager.h"
#include "nsString.h"

nsHashtable * NSApplicationManager::applications = NULL;
PRMonitor *NSApplicationManager::monitor = NULL;

class ApplicationEntry {
public:
  nsIApplicationShell * applicationshell;
  nsIShellInstance * shellinstance;

  ApplicationEntry(nsIApplicationShell * aApplicationShell, 
                   nsIShellInstance * aShellInstance) { 
    applicationshell = aApplicationShell;
    shellinstance = aShellInstance;
  }
  ~ApplicationEntry() {
  }
};

class AppKey: public nsHashKey {
private:
  nsIApplicationShell * applicationshell;
  
public:
  AppKey(nsIApplicationShell * aApplicationShell) {
    applicationshell = aApplicationShell;
  }
  
  PRUint32 HashValue(void) const {
    return ((PRUint32) (applicationshell));
  }

  PRBool Equals(const nsHashKey *aKey) const {
    return ((PRBool)(applicationshell == ((AppKey *)aKey)->applicationshell));
  }

  nsHashKey *Clone(void) const {
    return new AppKey(applicationshell);
  }
};



nsresult NSApplicationManager::GetShellInstance(nsIApplicationShell * aApplicationShell,
                                             nsIShellInstance **aShellInstance) 
{
  checkInitialized();

  if (aShellInstance == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  PR_EnterMonitor(monitor);

  AppKey key(aApplicationShell);
  ApplicationEntry *entry = (ApplicationEntry*) applications->Get(&key);

  nsresult res = NS_ERROR_FAILURE;

  PR_ExitMonitor(monitor);

  if (entry != NULL) {
      *aShellInstance = entry->shellinstance;
      (*aShellInstance)->AddRef();
      res = NS_OK;
  }

  return res;
}

nsresult NSApplicationManager::checkInitialized() 
{
  nsresult res = NS_OK;
  if (applications == NULL) {
    res = Initialize();
  }
  return res;
}

nsresult NSApplicationManager::Initialize() 
{
  if (applications == NULL) {
    applications = new nsHashtable();
  }
  if (monitor == NULL) {
    monitor = PR_NewMonitor();
  }

  return NS_OK;
}


nsresult NSApplicationManager::SetShellAssociation(nsIApplicationShell * aApplicationShell,
                                                   nsIShellInstance *aShellInstance)
{
  checkInitialized();

  nsIShellInstance *old = NULL;
  GetShellInstance(aApplicationShell, &old);
  
  if (old != NULL) {
    old->Release();
    return NS_ERROR_FAILURE;
  }

  PR_EnterMonitor(monitor);

  AppKey key(aApplicationShell);
  applications->Put(&key, new ApplicationEntry(aApplicationShell, aShellInstance));

  PR_ExitMonitor(monitor);

  return NS_OK;
}

nsresult NSApplicationManager::DeleteShellAssociation(nsIApplicationShell * aApplicationShell,
                                                      nsIShellInstance *aShellInstance)
{
  checkInitialized();

  nsIShellInstance *old = NULL;
  GetShellInstance(aApplicationShell, &old);

  nsresult res = NS_ERROR_FACTORY_NOT_REGISTERED;
  if (old != NULL) {
    if (old == aShellInstance) {
      PR_EnterMonitor(monitor);

      AppKey key(aApplicationShell);
      ApplicationEntry *entry = (ApplicationEntry *) applications->Remove(&key);
      delete entry;

      PR_ExitMonitor(monitor);
  
      res = NS_OK;
    }
    old->Release();
  }

  return res;
}

nsresult NSApplicationManager::ModalMessage(const nsString &aMessage, 
                                            const nsString &aTitle, 
                                            nsModalMessageType aModalMessageType)
{

  nsresult res = NS_OK ;

#ifdef NS_WIN32

  PRInt32 msgtype ;

  switch (aModalMessageType)
  {
    case eModalMessage_ok:
      msgtype = MB_OK ;
    break ;

    case eModalMessage_ok_cancel:
      msgtype = MB_OK ;
    break ;

    default:
      msgtype = MB_OK ;
    break ;

  }
  ::MessageBox(NULL, (const char *)aMessage.GetUnicode(), (const char *)aTitle.GetUnicode(), msgtype);

#endif

  return res ;
}
