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

#ifndef nsIDOMInstall_h__
#define nsIDOMInstall_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMInstallFolder;
class nsIDOMInstallVersion;

#define NS_IDOMINSTALL_IID \
 { 0x18c2f988, 0xb09f, 0x11d2, \
  {0xbc, 0xde, 0x00, 0x80, 0x5f, 0x0e, 0x13, 0x53}} 

class nsIDOMInstall : public nsISupports {
public:
  static const nsIID& IID() { static nsIID iid = NS_IDOMINSTALL_IID; return iid; }
  enum {
    SUERR_BAD_PACKAGE_NAME = -200,
    SUERR_UNEXPECTED_ERROR = -201,
    SUERR_ACCESS_DENIED = -202,
    SUERR_TOO_MANY_CERTIFICATES = -203,
    SUERR_NO_INSTALLER_CERTIFICATE = -204,
    SUERR_NO_CERTIFICATE = -205,
    SUERR_NO_MATCHING_CERTIFICATE = -206,
    SUERR_UNKNOWN_JAR_FILE = -207,
    SUERR_INVALID_ARGUMENTS = -208,
    SUERR_ILLEGAL_RELATIVE_PATH = -209,
    SUERR_USER_CANCELLED = -210,
    SUERR_INSTALL_NOT_STARTED = -211,
    SUERR_SILENT_MODE_DENIED = -212,
    SUERR_NO_SUCH_COMPONENT = -213,
    SUERR_FILE_DOES_NOT_EXIST = -214,
    SUERR_FILE_READ_ONLY = -215,
    SUERR_FILE_IS_DIRECTORY = -216,
    SUERR_NETWORK_FILE_IS_IN_USE = -217,
    SUERR_APPLE_SINGLE_ERR = -218,
    SUERR_INVALID_PATH_ERR = -219,
    SUERR_PATCH_BAD_DIFF = -220,
    SUERR_PATCH_BAD_CHECKSUM_TARGET = -221,
    SUERR_PATCH_BAD_CHECKSUM_RESULT = -222,
    SUERR_UNINSTALL_FAILED = -223,
    SUERR_GESTALT_UNKNOWN_ERR = -5550,
    SUERR_GESTALT_INVALID_ARGUMENT = -5551,
    SU_SUCCESS = 0,
    SU_REBOOT_NEEDED = 999,
    SU_LIMITED_INSTALL = 0,
    SU_FULL_INSTALL = 1,
    SU_NO_STATUS_DLG = 2,
    SU_NO_FINALIZE_DLG = 4,
    SU_INSTALL_FILE_UNEXPECTED_MSG_ID = 0,
    SU_DETAILS_REPLACE_FILE_MSG_ID = 1,
    SU_DETAILS_INSTALL_FILE_MSG_ID = 2
  };

  NS_IMETHOD    GetUserPackageName(nsString& aUserPackageName)=0;

  NS_IMETHOD    GetRegPackageName(nsString& aRegPackageName)=0;

  NS_IMETHOD    AbortInstall()=0;

  NS_IMETHOD    AddDirectory(const nsString& aRegName, const nsString& aVersion, const nsString& aJarSource, nsIDOMInstallFolder* aFolder, const nsString& aSubdir, PRBool aForceMode, PRInt32* aReturn)=0;

  NS_IMETHOD    AddSubcomponent(const nsString& aRegName, nsIDOMInstallVersion* aVersion, const nsString& aJarSource, nsIDOMInstallFolder* aFolder, const nsString& aTargetName, PRBool aForceMode, PRInt32* aReturn)=0;

  NS_IMETHOD    DeleteComponent(const nsString& aRegistryName, PRInt32* aReturn)=0;

  NS_IMETHOD    DeleteFile(nsIDOMInstallFolder* aFolder, const nsString& aRelativeFileName, PRInt32* aReturn)=0;

  NS_IMETHOD    DiskSpaceAvailable(nsIDOMInstallFolder* aFolder, PRInt32* aReturn)=0;

  NS_IMETHOD    Execute(const nsString& aJarSource, const nsString& aArgs, PRInt32* aReturn)=0;

  NS_IMETHOD    FinalizeInstall(PRInt32* aReturn)=0;

  NS_IMETHOD    Gestalt(const nsString& aSelector, PRInt32* aReturn)=0;

  NS_IMETHOD    GetComponentFolder(const nsString& aRegName, const nsString& aSubdirectory, nsIDOMInstallFolder** aReturn)=0;

  NS_IMETHOD    GetFolder(nsIDOMInstallFolder* aTargetFolder, const nsString& aSubdirectory, nsIDOMInstallFolder** aReturn)=0;

  NS_IMETHOD    GetLastError(PRInt32* aReturn)=0;

  NS_IMETHOD    GetWinProfile(nsIDOMInstallFolder* aFolder, const nsString& aFile, PRInt32* aReturn)=0;

  NS_IMETHOD    GetWinRegistry(PRInt32* aReturn)=0;

  NS_IMETHOD    Patch(const nsString& aRegName, const nsString& aVersion, const nsString& aJarSource, nsIDOMInstallFolder* aFolder, const nsString& aTargetName, PRInt32* aReturn)=0;

  NS_IMETHOD    ResetError()=0;

  NS_IMETHOD    SetPackageFolder(nsIDOMInstallFolder* aFolder)=0;

  NS_IMETHOD    StartInstall(const nsString& aUserPackageName, const nsString& aPackageName, const nsString& aVersion, PRInt32 aFlags, PRInt32* aReturn)=0;

  NS_IMETHOD    Uninstall(const nsString& aPackageName, PRInt32* aReturn)=0;

  NS_IMETHOD    ExtractFileFromJar(const nsString& aJarfile, const nsString& aFinalFile, nsString& aTempFile, nsString& aErrorMsg)=0;
};


#define NS_DECL_IDOMINSTALL   \
  NS_IMETHOD    GetUserPackageName(nsString& aUserPackageName);  \
  NS_IMETHOD    GetRegPackageName(nsString& aRegPackageName);  \
  NS_IMETHOD    AbortInstall();  \
  NS_IMETHOD    AddDirectory(const nsString& aRegName, const nsString& aVersion, const nsString& aJarSource, nsIDOMInstallFolder* aFolder, const nsString& aSubdir, PRBool aForceMode, PRInt32* aReturn);  \
  NS_IMETHOD    AddSubcomponent(const nsString& aRegName, nsIDOMInstallVersion* aVersion, const nsString& aJarSource, nsIDOMInstallFolder* aFolder, const nsString& aTargetName, PRBool aForceMode, PRInt32* aReturn);  \
  NS_IMETHOD    DeleteComponent(const nsString& aRegistryName, PRInt32* aReturn);  \
  NS_IMETHOD    DeleteFile(nsIDOMInstallFolder* aFolder, const nsString& aRelativeFileName, PRInt32* aReturn);  \
  NS_IMETHOD    DiskSpaceAvailable(nsIDOMInstallFolder* aFolder, PRInt32* aReturn);  \
  NS_IMETHOD    Execute(const nsString& aJarSource, const nsString& aArgs, PRInt32* aReturn);  \
  NS_IMETHOD    FinalizeInstall(PRInt32* aReturn);  \
  NS_IMETHOD    Gestalt(const nsString& aSelector, PRInt32* aReturn);  \
  NS_IMETHOD    GetComponentFolder(const nsString& aRegName, const nsString& aSubdirectory, nsIDOMInstallFolder** aReturn);  \
  NS_IMETHOD    GetFolder(nsIDOMInstallFolder* aTargetFolder, const nsString& aSubdirectory, nsIDOMInstallFolder** aReturn);  \
  NS_IMETHOD    GetLastError(PRInt32* aReturn);  \
  NS_IMETHOD    GetWinProfile(nsIDOMInstallFolder* aFolder, const nsString& aFile, PRInt32* aReturn);  \
  NS_IMETHOD    GetWinRegistry(PRInt32* aReturn);  \
  NS_IMETHOD    Patch(const nsString& aRegName, const nsString& aVersion, const nsString& aJarSource, nsIDOMInstallFolder* aFolder, const nsString& aTargetName, PRInt32* aReturn);  \
  NS_IMETHOD    ResetError();  \
  NS_IMETHOD    SetPackageFolder(nsIDOMInstallFolder* aFolder);  \
  NS_IMETHOD    StartInstall(const nsString& aUserPackageName, const nsString& aPackageName, const nsString& aVersion, PRInt32 aFlags, PRInt32* aReturn);  \
  NS_IMETHOD    Uninstall(const nsString& aPackageName, PRInt32* aReturn);  \
  NS_IMETHOD    ExtractFileFromJar(const nsString& aJarfile, const nsString& aFinalFile, nsString& aTempFile, nsString& aErrorMsg);  \



#define NS_FORWARD_IDOMINSTALL(_to)  \
  NS_IMETHOD    GetUserPackageName(nsString& aUserPackageName) { return _to##GetUserPackageName(aUserPackageName); } \
  NS_IMETHOD    GetRegPackageName(nsString& aRegPackageName) { return _to##GetRegPackageName(aRegPackageName); } \
  NS_IMETHOD    AbortInstall() { return _to##AbortInstall(); }  \
  NS_IMETHOD    AddDirectory(const nsString& aRegName, const nsString& aVersion, const nsString& aJarSource, nsIDOMInstallFolder* aFolder, const nsString& aSubdir, PRBool aForceMode, PRInt32* aReturn) { return _to##AddDirectory(aRegName, aVersion, aJarSource, aFolder, aSubdir, aForceMode, aReturn); }  \
  NS_IMETHOD    AddSubcomponent(const nsString& aRegName, nsIDOMInstallVersion* aVersion, const nsString& aJarSource, nsIDOMInstallFolder* aFolder, const nsString& aTargetName, PRBool aForceMode, PRInt32* aReturn) { return _to##AddSubcomponent(aRegName, aVersion, aJarSource, aFolder, aTargetName, aForceMode, aReturn); }  \
  NS_IMETHOD    DeleteComponent(const nsString& aRegistryName, PRInt32* aReturn) { return _to##DeleteComponent(aRegistryName, aReturn); }  \
  NS_IMETHOD    DeleteFile(nsIDOMInstallFolder* aFolder, const nsString& aRelativeFileName, PRInt32* aReturn) { return _to##DeleteFile(aFolder, aRelativeFileName, aReturn); }  \
  NS_IMETHOD    DiskSpaceAvailable(nsIDOMInstallFolder* aFolder, PRInt32* aReturn) { return _to##DiskSpaceAvailable(aFolder, aReturn); }  \
  NS_IMETHOD    Execute(const nsString& aJarSource, const nsString& aArgs, PRInt32* aReturn) { return _to##Execute(aJarSource, aArgs, aReturn); }  \
  NS_IMETHOD    FinalizeInstall(PRInt32* aReturn) { return _to##FinalizeInstall(aReturn); }  \
  NS_IMETHOD    Gestalt(const nsString& aSelector, PRInt32* aReturn) { return _to##Gestalt(aSelector, aReturn); }  \
  NS_IMETHOD    GetComponentFolder(const nsString& aRegName, const nsString& aSubdirectory, nsIDOMInstallFolder** aReturn) { return _to##GetComponentFolder(aRegName, aSubdirectory, aReturn); }  \
  NS_IMETHOD    GetFolder(nsIDOMInstallFolder* aTargetFolder, const nsString& aSubdirectory, nsIDOMInstallFolder** aReturn) { return _to##GetFolder(aTargetFolder, aSubdirectory, aReturn); }  \
  NS_IMETHOD    GetLastError(PRInt32* aReturn) { return _to##GetLastError(aReturn); }  \
  NS_IMETHOD    GetWinProfile(nsIDOMInstallFolder* aFolder, const nsString& aFile, PRInt32* aReturn) { return _to##GetWinProfile(aFolder, aFile, aReturn); }  \
  NS_IMETHOD    GetWinRegistry(PRInt32* aReturn) { return _to##GetWinRegistry(aReturn); }  \
  NS_IMETHOD    Patch(const nsString& aRegName, const nsString& aVersion, const nsString& aJarSource, nsIDOMInstallFolder* aFolder, const nsString& aTargetName, PRInt32* aReturn) { return _to##Patch(aRegName, aVersion, aJarSource, aFolder, aTargetName, aReturn); }  \
  NS_IMETHOD    ResetError() { return _to##ResetError(); }  \
  NS_IMETHOD    SetPackageFolder(nsIDOMInstallFolder* aFolder) { return _to##SetPackageFolder(aFolder); }  \
  NS_IMETHOD    StartInstall(const nsString& aUserPackageName, const nsString& aPackageName, const nsString& aVersion, PRInt32 aFlags, PRInt32* aReturn) { return _to##StartInstall(aUserPackageName, aPackageName, aVersion, aFlags, aReturn); }  \
  NS_IMETHOD    Uninstall(const nsString& aPackageName, PRInt32* aReturn) { return _to##Uninstall(aPackageName, aReturn); }  \
  NS_IMETHOD    ExtractFileFromJar(const nsString& aJarfile, const nsString& aFinalFile, nsString& aTempFile, nsString& aErrorMsg) { return _to##ExtractFileFromJar(aJarfile, aFinalFile, aTempFile, aErrorMsg); }  \


extern nsresult NS_InitInstallClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptInstall(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMInstall_h__
