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

#include "xp.h"
#include "xpgetstr.h"
#include "prprf.h"
#include "nsSUError.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


char * SU_GetErrorMsg1(int id, char* arg1) 
{
  char* errorMsg=NULL;
  char* tag = XP_GetString(id);
  PR_ASSERT(tag != NULL);
  errorMsg = PR_sprintf_append(errorMsg, tag, arg1);
  return errorMsg;
}

char * SU_GetErrorMsg2(int id, nsString* arg1) 
{
  char* errorMsg=NULL;
  char* tag = XP_GetString(id);
  PR_ASSERT(tag != NULL);
  char* argMsg = arg1->ToNewCString();
  errorMsg = PR_sprintf_append(errorMsg, tag, argMsg);
  delete argMsg;
  return errorMsg;
}

char * SU_GetErrorMsg3(char *str, int err) 
{
  char* errorMsg=NULL;
  PR_ASSERT(str != NULL);
  errorMsg = PR_sprintf_append(errorMsg, "%s %d", str, err);
  return errorMsg;
}

char * SU_GetString(int id) 
{
  char *str = XP_GetString(id);
  PR_ASSERT(str != NULL);
  return XP_STRDUP(str);
}

char * SU_GetString1(int id, char* arg1) 
{
  char* msg=NULL;
  char* tag = XP_GetString(id);
  PR_ASSERT(tag != NULL);
  msg = PR_sprintf_append(msg, tag, arg1);
  return msg;
}

char * SU_GetString2(int id, nsString* arg1) 
{
  char* msg=NULL;
  char* tag = XP_GetString(id);
  PR_ASSERT(tag != NULL);
  char* argMsg = arg1->ToNewCString();
  msg = PR_sprintf_append(msg, tag, argMsg);
  delete argMsg;
  return msg;
}


#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
