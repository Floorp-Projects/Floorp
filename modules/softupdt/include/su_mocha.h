/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Daniel Veditz <dveditz@netscape.com>
 */

#ifndef SU_MOCHA_H
#define SU_MOCHA_H

#include "VerReg.h"

/* trigger modes */
#define SU_DEFAULT_MODE     0
#define SU_FORCE_MODE       1
#define SU_SILENT_MODE      2

/* version compare values */
#define SU_EQUAL            0
#define SU_BLD_DIFF         1
#define SU_REL_DIFF         2
#define SU_MINOR_DIFF       3
#define SU_MAJOR_DIFF       4

/* StartInstall() modes */
#define SU_LIMITED_INSTALL  0
#define SU_FULL_INSTALL     1
#define SU_NO_STATUS_DLG    2
#define SU_NO_FINALIZE_DLG  4

/* SoftUpdate return values */
extern char su_BAD_PACKAGE_NAME_str[];
extern char su_UNEXPECTED_ERROR_str[];
extern char su_ACCESS_DENIED_str[];
extern char su_TOO_MANY_CERTIFICATES_str[];
extern char su_NO_INSTALLER_CERTIFICATE_str[];
extern char su_NO_CERTIFICATE_str[];
extern char su_NO_MATCHING_CERTIFICATE_str[];
extern char su_UNKNOWN_JAR_FILE_str[];
extern char su_INVALID_ARGUMENTS_str[];
extern char su_ILLEGAL_RELATIVE_PATH_str[];
extern char su_USER_CANCELLED_str[];
extern char su_INSTALL_NOT_STARTED_str[];
extern char su_SILENT_MODE_DENIED_str[];
extern char su_NO_SUCH_COMPONENT_str[];
extern char su_FILE_DOES_NOT_EXIST_str[];
extern char su_FILE_READ_ONLY_str[];
extern char su_FILE_IS_DIRECTORY_str[];
extern char su_NETWORK_FILE_IS_IN_USE_str[];
extern char su_APPLE_SINGLE_ERR_str[];
extern char su_INVALID_PATH_ERR_str[];
extern char su_PATCH_BAD_DIFF_str[];
extern char su_PATCH_BAD_CHECKSUM_TARGET_str[];
extern char su_PATCH_BAD_CHECKSUM_RESULT_str[];
extern char su_UNINSTALL_FAILED_str[];
extern char su_SUCCESS_str[];
extern char su_REBOOT_NEEDED_str[];


XP_BEGIN_PROTOS


extern char su_defaultMode_str[];
extern char su_forceMode_str[];
extern char su_silentMode_str[];
extern char su_equal_str[];
extern char su_bldDiff_str[];
extern char su_relDiff_str[];
extern char su_minorDiff_str[];
extern char su_majorDiff_str[];
extern char su_major_str[];
extern char su_minor_str[];
extern char su_release_str[];
extern char su_build_str[];

extern JSClass su_version_class;

/* create Software Update objects in given JS context */
extern JSBool SU_InitMochaClasses(JSContext *cx, JSObject *obj);

/* individual class definition, not public */
extern JSBool su_DefineInstall(JSContext *cx, JSObject *obj);
extern JSBool su_DefineTrigger(JSContext *cx, JSObject *obj);
extern JSBool su_DefineVersion(JSContext *cx, JSObject *obj);

/* convert character string to VERSION structure */
extern void su_strToVersion(char * verstr, VERSION* vers);

/* compare two VERSION structures */
int su_compareVersions(VERSION* vers1, VERSION* vers2);

/* converters between VERSION structures and InstallVersion objects */
void su_versToObj(JSContext *cx, VERSION* vers, JSObject* versObj);
void su_objToVers(JSContext *cx, JSObject* versObj, VERSION* vers);

XP_END_PROTOS

#endif /* SU_MOCHA_H */