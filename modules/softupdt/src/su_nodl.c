/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "prtypes.h"

#include "prlink.h"

extern void Java_netscape_softupdate_FolderSpec_GetNativePath_stub();
extern void Java_netscape_softupdate_FolderSpec_GetSecurityTargetID_stub();
extern void Java_netscape_softupdate_FolderSpec_NativeGetDirectoryPath_stub();
extern void Java_netscape_softupdate_FolderSpec_NativePickDefaultDirectory_stub();
extern void Java_netscape_softupdate_InstallExecute_NativeComplete_stub();
extern void Java_netscape_softupdate_InstallFile_NativeComplete_stub();
extern void Java_netscape_softupdate_RegEntryEnumerator_regNext_stub();
extern void Java_netscape_softupdate_RegKeyEnumerator_regNext_stub();
extern void Java_netscape_softupdate_RegistryNode_nDeleteEntry_stub();
extern void Java_netscape_softupdate_RegistryNode_nGetEntryType_stub();
extern void Java_netscape_softupdate_RegistryNode_nGetEntry_stub();
extern void Java_netscape_softupdate_RegistryNode_setEntryB_stub();
extern void Java_netscape_softupdate_RegistryNode_setEntryI_stub();
extern void Java_netscape_softupdate_RegistryNode_setEntryS_stub();
extern void Java_netscape_softupdate_Registry_nAddKey_stub();
extern void Java_netscape_softupdate_Registry_nClose_stub();
extern void Java_netscape_softupdate_Registry_nDeleteKey_stub();
extern void Java_netscape_softupdate_Registry_nGetKey_stub();
extern void Java_netscape_softupdate_Registry_nOpen_stub();
extern void Java_netscape_softupdate_Registry_nUserName_stub();
extern void Java_netscape_softupdate_SoftwareUpdate_CloseJARFile_stub();
extern void Java_netscape_softupdate_SoftwareUpdate_NativeExtractJARFile_stub();
extern void Java_netscape_softupdate_SoftwareUpdate_OpenJARFile_stub();
extern void Java_netscape_softupdate_SoftwareUpdate_VerifyJSObject_stub();
extern void Java_netscape_softupdate_SoftwareUpdate_getCertificates_stub();
extern void Java_netscape_softupdate_SoftwareUpdate_NativeGestalt_stub();
extern void Java_netscape_softupdate_SoftwareUpdate_NativeDeleteFile_stub();
extern void Java_netscape_softupdate_SoftwareUpdate_NativeVerifyDiskspace_stub();
extern void Java_netscape_softupdate_SoftwareUpdate_NativeMakeDirectory_stub();
extern void Java_netscape_softupdate_SoftwareUpdate_ExtractDirEntries_stub();
extern void Java_netscape_softupdate_Trigger_StartSoftwareUpdate_stub();
extern void Java_netscape_softupdate_Trigger_UpdateEnabled_stub();
extern void Java_netscape_softupdate_VerRegEnumerator_regNext_stub();
extern void Java_netscape_softupdate_VersionRegistry_close_stub();
extern void Java_netscape_softupdate_VersionRegistry_componentPath_stub();
extern void Java_netscape_softupdate_VersionRegistry_componentVersion_stub();
extern void Java_netscape_softupdate_VersionRegistry_deleteComponent_stub();
extern void Java_netscape_softupdate_VersionRegistry_getDefaultDirectory_stub();
extern void Java_netscape_softupdate_VersionRegistry_inRegistry_stub();
extern void Java_netscape_softupdate_VersionRegistry_installComponent_stub();
extern void Java_netscape_softupdate_VersionRegistry_setDefaultDirectory_stub();
extern void Java_netscape_softupdate_VersionRegistry_validateComponent_stub();
extern void Java_netscape_softupdate_InstallExecute_NativeAbort_stub();
extern void Java_netscape_softupdate_InstallFile_NativeAbort_stub();

#ifdef XP_PC
extern void Java_netscape_softupdate_WinProfile_nativeWriteString_stub();
extern void Java_netscape_softupdate_WinProfile_nativeGetString_stub();
extern void Java_netscape_softupdate_WinReg_nativeCreateKey_stub();
extern void Java_netscape_softupdate_WinReg_nativeDeleteKey_stub();
extern void Java_netscape_softupdate_WinReg_nativeDeleteValue_stub();
extern void Java_netscape_softupdate_WinReg_nativeSetValueString_stub();
extern void Java_netscape_softupdate_WinReg_nativeGetValueString_stub();
extern void Java_netscape_softupdate_WinReg_nativeSetValue_stub();
extern void Java_netscape_softupdate_WinReg_nativeGetValue_stub();
#endif

PRStaticLinkTable su_nodl_tab[] = {
  { "Java_netscape_softupdate_FolderSpec_GetNativePath_stub", Java_netscape_softupdate_FolderSpec_GetNativePath_stub },
  { "Java_netscape_softupdate_FolderSpec_GetSecurityTargetID_stub", Java_netscape_softupdate_FolderSpec_GetSecurityTargetID_stub },
  { "Java_netscape_softupdate_FolderSpec_NativeGetDirectoryPath_stub", Java_netscape_softupdate_FolderSpec_NativeGetDirectoryPath_stub },
  { "Java_netscape_softupdate_FolderSpec_NativePickDefaultDirectory_stub", Java_netscape_softupdate_FolderSpec_NativePickDefaultDirectory_stub },
  { "Java_netscape_softupdate_InstallExecute_NativeComplete_stub", Java_netscape_softupdate_InstallExecute_NativeComplete_stub },
  { "Java_netscape_softupdate_InstallFile_NativeComplete_stub", Java_netscape_softupdate_InstallFile_NativeComplete_stub },
  { "Java_netscape_softupdate_RegEntryEnumerator_regNext_stub", Java_netscape_softupdate_RegEntryEnumerator_regNext_stub },
  { "Java_netscape_softupdate_RegKeyEnumerator_regNext_stub", Java_netscape_softupdate_RegKeyEnumerator_regNext_stub },
  { "Java_netscape_softupdate_RegistryNode_nDeleteEntry_stub", Java_netscape_softupdate_RegistryNode_nDeleteEntry_stub },
  { "Java_netscape_softupdate_RegistryNode_nGetEntryType_stub", Java_netscape_softupdate_RegistryNode_nGetEntryType_stub },
  { "Java_netscape_softupdate_RegistryNode_nGetEntry_stub", Java_netscape_softupdate_RegistryNode_nGetEntry_stub },
  { "Java_netscape_softupdate_RegistryNode_setEntryB_stub", Java_netscape_softupdate_RegistryNode_setEntryB_stub },
  { "Java_netscape_softupdate_RegistryNode_setEntryI_stub", Java_netscape_softupdate_RegistryNode_setEntryI_stub },
  { "Java_netscape_softupdate_RegistryNode_setEntryS_stub", Java_netscape_softupdate_RegistryNode_setEntryS_stub },
  { "Java_netscape_softupdate_Registry_nAddKey_stub", Java_netscape_softupdate_Registry_nAddKey_stub },
  { "Java_netscape_softupdate_Registry_nClose_stub", Java_netscape_softupdate_Registry_nClose_stub },
  { "Java_netscape_softupdate_Registry_nDeleteKey_stub", Java_netscape_softupdate_Registry_nDeleteKey_stub },
  { "Java_netscape_softupdate_Registry_nGetKey_stub", Java_netscape_softupdate_Registry_nGetKey_stub },
  { "Java_netscape_softupdate_Registry_nOpen_stub", Java_netscape_softupdate_Registry_nOpen_stub },
  { "Java_netscape_softupdate_Registry_nUserName_stub", Java_netscape_softupdate_Registry_nUserName_stub },
  { "Java_netscape_softupdate_SoftwareUpdate_CloseJARFile_stub", Java_netscape_softupdate_SoftwareUpdate_CloseJARFile_stub },
  { "Java_netscape_softupdate_SoftwareUpdate_NativeExtractJARFile_stub", Java_netscape_softupdate_SoftwareUpdate_NativeExtractJARFile_stub },
  { "Java_netscape_softupdate_SoftwareUpdate_OpenJARFile_stub", Java_netscape_softupdate_SoftwareUpdate_OpenJARFile_stub },
  { "Java_netscape_softupdate_SoftwareUpdate_VerifyJSObject_stub", Java_netscape_softupdate_SoftwareUpdate_VerifyJSObject_stub },
  { "Java_netscape_softupdate_SoftwareUpdate_getCertificates_stub", Java_netscape_softupdate_SoftwareUpdate_getCertificates_stub },
  { "Java_netscape_softupdate_SoftwareUpdate_NativeGestalt_stub", Java_netscape_softupdate_SoftwareUpdate_NativeGestalt_stub },
  { "Java_netscape_softupdate_SoftwareUpdate_NativeDeleteFile_stub", Java_netscape_softupdate_SoftwareUpdate_NativeDeleteFile_stub },
  { "Java_netscape_softupdate_SoftwareUpdate_NativeVerifyDiskspace_stub", Java_netscape_softupdate_SoftwareUpdate_NativeVerifyDiskspace_stub },
  { "Java_netscape_softupdate_SoftwareUpdate_NativeMakeDirectory_stub", Java_netscape_softupdate_SoftwareUpdate_NativeMakeDirectory_stub },
  { "Java_netscape_softupdate_SoftwareUpdate_ExtractDirEntries_stub", Java_netscape_softupdate_SoftwareUpdate_ExtractDirEntries_stub },
  { "Java_netscape_softupdate_Trigger_StartSoftwareUpdate_stub", Java_netscape_softupdate_Trigger_StartSoftwareUpdate_stub },
  { "Java_netscape_softupdate_Trigger_UpdateEnabled_stub", Java_netscape_softupdate_Trigger_UpdateEnabled_stub },
  { "Java_netscape_softupdate_VerRegEnumerator_regNext_stub", Java_netscape_softupdate_VerRegEnumerator_regNext_stub },
  { "Java_netscape_softupdate_VersionRegistry_close_stub", Java_netscape_softupdate_VersionRegistry_close_stub },
  { "Java_netscape_softupdate_VersionRegistry_componentPath_stub", Java_netscape_softupdate_VersionRegistry_componentPath_stub },
  { "Java_netscape_softupdate_VersionRegistry_componentVersion_stub", Java_netscape_softupdate_VersionRegistry_componentVersion_stub },
  { "Java_netscape_softupdate_VersionRegistry_deleteComponent_stub", Java_netscape_softupdate_VersionRegistry_deleteComponent_stub },
  { "Java_netscape_softupdate_VersionRegistry_getDefaultDirectory_stub", Java_netscape_softupdate_VersionRegistry_getDefaultDirectory_stub },
  { "Java_netscape_softupdate_VersionRegistry_inRegistry_stub", Java_netscape_softupdate_VersionRegistry_inRegistry_stub },
  { "Java_netscape_softupdate_VersionRegistry_installComponent_stub", Java_netscape_softupdate_VersionRegistry_installComponent_stub },
  { "Java_netscape_softupdate_VersionRegistry_setDefaultDirectory_stub", Java_netscape_softupdate_VersionRegistry_setDefaultDirectory_stub },
  { "Java_netscape_softupdate_VersionRegistry_validateComponent_stub", Java_netscape_softupdate_VersionRegistry_validateComponent_stub },
  { "Java_netscape_softupdate_InstallExecute_NativeAbort_stub", Java_netscape_softupdate_InstallExecute_NativeAbort_stub },
  { "Java_netscape_softupdate_InstallFile_NativeAbort_stub", Java_netscape_softupdate_InstallFile_NativeAbort_stub },
#ifdef XP_PC
  { "Java_netscape_softupdate_WinProfile_nativeWriteString_stub", Java_netscape_softupdate_WinProfile_nativeWriteString_stub },
  { "Java_netscape_softupdate_WinProfile_nativeGetString_stub", Java_netscape_softupdate_WinProfile_nativeGetString_stub },
  { "Java_netscape_softupdate_WinReg_nativeCreateKey_stub", Java_netscape_softupdate_WinReg_nativeCreateKey_stub },
  { "Java_netscape_softupdate_WinReg_nativeDeleteKey_stub", Java_netscape_softupdate_WinReg_nativeDeleteKey_stub },
  { "Java_netscape_softupdate_WinReg_nativeDeleteValue_stub", Java_netscape_softupdate_WinReg_nativeDeleteValue_stub },
  { "Java_netscape_softupdate_WinReg_nativeSetValueString_stub", Java_netscape_softupdate_WinReg_nativeSetValueString_stub },
  { "Java_netscape_softupdate_WinReg_nativeGetValueString_stub", Java_netscape_softupdate_WinReg_nativeGetValueString_stub },
  { "Java_netscape_softupdate_WinReg_nativeSetValue_stub", Java_netscape_softupdate_WinReg_nativeSetValue_stub },
  { "Java_netscape_softupdate_WinReg_nativeGetValue_stub", Java_netscape_softupdate_WinReg_nativeGetValue_stub },
#endif
  { 0, 0, },
};
