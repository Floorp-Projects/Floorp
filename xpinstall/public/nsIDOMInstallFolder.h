/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
/* AUTO-GENERATED. DO NOT EDIT!!! */

#ifndef nsIDOMInstallFolder_h__
#define nsIDOMInstallFolder_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"


#define NS_IDOMINSTALLFOLDER_IID \
 { 0x18c2f993, 0xb09f, 0x11d2, \
  {0xbc, 0xde, 0x00, 0x80, 0x5f, 0x0e, 0x13, 0x53}} 

class nsIDOMInstallFolder : public nsISupports {
public:
  static const nsIID& IID() { static nsIID iid = NS_IDOMINSTALLFOLDER_IID; return iid; }
  enum {
    BadFolder = 0,
    PluginFolder = 1,
    ProgramFolder = 2,
    PackageFolder = 3,
    TemporaryFolder = 4,
    CommunicatorFolder = 5,
    InstalledFolder = 6,
    CurrentUserFolder = 7,
    NetHelpFolder = 8,
    OSDriveFolder = 9,
    FileURLFolder = 10,
    JavaBinFolder = 11,
    JavaClassesFolder = 12,
    JavaDownloadFolder = 13,
    Win_WindowsFolder = 14,
    Win_SystemFolder = 15,
    Win_System16Folder = 16,
    Mac_SystemFolder = 17,
    Mac_DesktopFolder = 18,
    Mac_TrashFolder = 19,
    Mac_StartupFolder = 20,
    Mac_ShutdownFolder = 21,
    Mac_AppleMenuFolder = 22,
    Mac_ControlPanelFolder = 23,
    Mac_ExtensionFolder = 24,
    Mac_FontsFolder = 25,
    Mac_PreferencesFolder = 26,
    Unix_LocalFolder = 27,
    Unix_LibFolder = 28
  };

  NS_IMETHOD    Init(const nsString& aFolderID, const nsString& aVrPatch, const nsString& aPackageName)=0;

  NS_IMETHOD    GetDirectoryPath(nsString& aDirectoryPath)=0;

  NS_IMETHOD    MakeFullPath(const nsString& aRelativePath, nsString& aFullPath)=0;

  NS_IMETHOD    IsJavaCapable(PRBool* aReturn)=0;

  NS_IMETHOD    ToString(nsString& aFolderString)=0;
};


#define NS_DECL_IDOMINSTALLFOLDER   \
  NS_IMETHOD    Init(const nsString& aFolderID, const nsString& aVrPatch, const nsString& aPackageName);  \
  NS_IMETHOD    GetDirectoryPath(nsString& aDirectoryPath);  \
  NS_IMETHOD    MakeFullPath(const nsString& aRelativePath, nsString& aFullPath);  \
  NS_IMETHOD    IsJavaCapable(PRBool* aReturn);  \
  NS_IMETHOD    ToString(nsString& aFolderString);  \



#define NS_FORWARD_IDOMINSTALLFOLDER(_to)  \
  NS_IMETHOD    Init(const nsString& aFolderID, const nsString& aVrPatch, const nsString& aPackageName) { return _to##Init(aFolderID, aVrPatch, aPackageName); }  \
  NS_IMETHOD    GetDirectoryPath(nsString& aDirectoryPath) { return _to##GetDirectoryPath(aDirectoryPath); }  \
  NS_IMETHOD    MakeFullPath(const nsString& aRelativePath, nsString& aFullPath) { return _to##MakeFullPath(aRelativePath, aFullPath); }  \
  NS_IMETHOD    IsJavaCapable(PRBool* aReturn) { return _to##IsJavaCapable(aReturn); }  \
  NS_IMETHOD    ToString(nsString& aFolderString) { return _to##ToString(aFolderString); }  \


extern nsresult NS_InitInstallFolderClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptInstallFolder(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMInstallFolder_h__
