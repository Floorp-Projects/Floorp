/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Veditz <dveditz@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
/* VerReg.h
 * XP Version Registry functions
 */
#ifndef _VERREG_H_
#define _VERREG_H_

#include "NSReg.h"

typedef struct _version
{
   int32   major;
   int32   minor;
   int32   release;
   int32   build;
   int32   check;
} VERSION;


/* CreateRegistry flags */
#define CR_NEWREGISTRY 1

PR_BEGIN_EXTERN_C
/* ---------------------------------------------------------------------
 * Version Registry Operations
 * ---------------------------------------------------------------------
 */
/* global registry operations */
/* VR_CreateRegistry is available only in the STANDALONE_REGISTRY builds */
VR_INTERFACE(REGERR) VR_CreateRegistry(char *installation, char *programPath, char *versionStr);
VR_INTERFACE(REGERR) VR_SetRegDirectory(const char *path);
VR_INTERFACE(REGERR) VR_PackRegistry(void *userData,  nr_RegPackCallbackFunc pdCallbackFunction);
VR_INTERFACE(REGERR) VR_Close(void);

/* component-level functions */
VR_INTERFACE(REGERR) VR_Install(char *component_path, char *filepath, char *version, int bDirectory);
VR_INTERFACE(REGERR) VR_Remove(char *component_path);
VR_INTERFACE(REGERR) VR_InRegistry(char *path);
VR_INTERFACE(REGERR) VR_ValidateComponent(char *path);
VR_INTERFACE(REGERR) VR_Enum(char *component_path, REGENUM *state, char *buffer, uint32 buflen);

/* dealing with parts of individual components */
VR_INTERFACE(REGERR) VR_GetVersion(char *component_path, VERSION *result);
VR_INTERFACE(REGERR) VR_GetPath(char *component_path, uint32 sizebuf, char *buf);
VR_INTERFACE(REGERR) VR_SetRefCount(char *component_path, int refcount);
VR_INTERFACE(REGERR) VR_GetRefCount(char *component_path, int *result);
VR_INTERFACE(REGERR) VR_GetDefaultDirectory(char *component_path, uint32 sizebuf, char *buf);
VR_INTERFACE(REGERR) VR_SetDefaultDirectory(char *component_path, char *directory);

/* uninstall functions */
VR_INTERFACE(REGERR) VR_UninstallCreateNode(char *regPackageName, char *userPackageName);
VR_INTERFACE(REGERR) VR_UninstallAddFileToList(char *regPackageName, char *vrName);
VR_INTERFACE(REGERR) VR_UninstallFileExistsInList(char *regPackageName, char *vrName);
VR_INTERFACE(REGERR) VR_UninstallEnumSharedFiles(char *component_path, REGENUM *state, char *buffer, uint32 buflen);
VR_INTERFACE(REGERR) VR_UninstallDeleteFileFromList(char *component_path, char *vrName);
VR_INTERFACE(REGERR) VR_UninstallDeleteSharedFilesKey(char *regPackageName);
VR_INTERFACE(REGERR) VR_UninstallDestroy(char *regPackageName);
VR_INTERFACE(REGERR) VR_EnumUninstall(REGENUM *state, char* userPackageName,
                                    int32 len1, char*regPackageName, int32 len2, PRBool bSharedList);
VR_INTERFACE(REGERR) VR_GetUninstallUserName(char *regPackageName, char *outbuf, uint32 buflen);

PR_END_EXTERN_C

#endif   /* _VERREG_H_ */

/* EOF: VerReg.h */

