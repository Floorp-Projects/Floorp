/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsSUError_h__
#define nsSUError_h__

#include "prtypes.h"
#include "nsString.h"
#include "nsSoftUpdateEnums.h"


PR_BEGIN_EXTERN_C

/* XXX: First cut at Error messages. The following could change. */
char * SU_GetErrorMsg1(int id, char* arg1);
char * SU_GetErrorMsg2(int id, nsString* arg1, int reason);
char * SU_GetErrorMsg3(char *str, int err);
char * SU_GetErrorMsg4(int id, int reason);
char * SU_GetString(int id);
char * SU_GetString1(int id, char* arg1);
char * SU_GetString2(int id, nsString* arg1);

PR_END_EXTERN_C

#endif /* nsSUError_h__ */
