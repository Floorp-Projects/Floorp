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
/* VerReg.h
 * XP Version Registry functions (prototype)
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

XP_BEGIN_PROTOS
/* ---------------------------------------------------------------------
 * Version Registry Operations
 * ---------------------------------------------------------------------
 */
VR_INTERFACE(REGERR) VR_Close(void);
VR_INTERFACE(REGERR) VR_PackRegistry(void);
VR_INTERFACE(REGERR) VR_CreateRegistry(char *installation, char *programPath, char *versionStr);
VR_INTERFACE(REGERR) VR_GetVersion(char *component_path, VERSION *result);
VR_INTERFACE(REGERR) VR_GetPath(char *component_path, uint32 sizebuf, char *buf);
VR_INTERFACE(REGERR) VR_GetDefaultDirectory(char *component_path, uint32 sizebuf, char *buf);
VR_INTERFACE(REGERR) VR_SetDefaultDirectory(char *component_path, char *directory);
VR_INTERFACE(REGERR) VR_Install(char *component_path, char *filepath, char *version, int bDirectory);
VR_INTERFACE(REGERR) VR_Remove(char *component_path);
VR_INTERFACE(REGERR) VR_InRegistry(char *path);
VR_INTERFACE(REGERR) VR_ValidateComponent(char *path);
VR_INTERFACE(REGERR) VR_Enum(REGENUM *state, char *buffer, uint32 buflen);


#ifndef STANDALONE_REGISTRY
VR_INTERFACE(void) VR_Initialize(void* env);
#endif

XP_END_PROTOS

#endif   /* _VERREG_H_ */

/* EOF: VerReg.h */

