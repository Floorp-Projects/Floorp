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
#include "structs.h"

/* Public */
typedef void (*SoftUpdateCompletionFunction) (int result, void * closure);

XP_BEGIN_PROTOS

/* Flags for start software update */
/* See trigger.java for docs */
#define FORCE_INSTALL 1
#define SILENT_INSTALL 2

/* StartSoftwareUpdate
 * performs the update, and calls the callback function with the 
 */
extern XP_Bool SU_StartSoftwareUpdate(MWContext * context, 
						const char * url, 
						const char * name, 
						SoftUpdateCompletionFunction f,
						void * completionClosure,
                        int32 flags); /* FORCE_INSTALL, SILENT_INSTALL */

/* SU_NewStream
 * Stream decoder for software updates
 */	
NET_StreamClass * SU_NewStream (int format_out, void * registration,
								URL_Struct * request, MWContext *context);

int PR_CALLBACK JavaGetBoolPref(char *pref_name);
int PR_CALLBACK IsJavaSecurityEnabled();
int PR_CALLBACK IsJavaSecurityDefaultTo30Enabled();

#define AUTOUPDATE_ENABLE_PREF "autoupdate.enabled"
#define CONTENT_ENCODING_HEADER "Content-encoding"
#define INSTALLER_HEADER "Install-Script"

XP_END_PROTOS
