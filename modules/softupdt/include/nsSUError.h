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
