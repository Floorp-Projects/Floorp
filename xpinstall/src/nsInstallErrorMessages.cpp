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
#include "plstr.h"
#include "prmem.h"

#include "nsString.h"
#include "nsInstallErrorMessages.h"



char * 
nsInstallErrorMessages::GetErrorMsg(int id, char* arg1) 
{
  char* errorMsg = "";
  char* tag = XP_GetString(id);
  PR_ASSERT(tag != NULL);
  errorMsg = PL_strcat(tag, arg1);
  return errorMsg;
}


char * 
nsInstallErrorMessages::GetErrorMsg(int id, nsString* arg1, int reason) 
{
  char* errorMsg = "";
  char* ptr;
  char* tag = XP_GetString(id);
  PR_ASSERT(tag != NULL);
  char* argMsg = "";
  if (argMsg)
    argMsg = arg1->ToNewCString();
  ptr = PR_sprintf_append(errorMsg, "%s %s %d", tag, argMsg, reason);
  delete argMsg;
  errorMsg = PL_strdup(ptr);
  delete ptr;
  return errorMsg;
}

char * 
nsInstallErrorMessages::GetErrorMsg(char *str, int err) 
{
  char* errorMsg = "";
  char* ptr;
  PR_ASSERT(str != NULL);
  ptr = PR_sprintf_append(errorMsg, "%s %d", str, err);
  errorMsg = PL_strdup(ptr);
  delete ptr;
  return errorMsg;
}

char * 
nsInstallErrorMessages::GetErrorMsg(int id, int reason) 
{
  char* msg;
  char* ptr;
  char* tag = XP_GetString(id);
  PR_ASSERT(tag != NULL);
  ptr = PR_sprintf_append(msg, "%s %d", tag, reason);
  msg = PL_strdup(ptr);
  delete ptr;
  return msg;
}


char * 
nsInstallErrorMessages::GetString(int id) 
{
  char *str = XP_GetString(id);
  PR_ASSERT(str != NULL);
  return PL_strdup(str);
}

char * 
nsInstallErrorMessages::GetString(int id, char* arg1) 
{
  char* msg=NULL;
  char* tag = XP_GetString(id);
  PR_ASSERT(tag != NULL);
  if (arg1) 
  {
    msg = PL_strcat(msg, tag);
    msg = PL_strcat(msg, arg1);
  }
  else 
  {
    msg = PL_strcat(msg, tag);
  }
  return msg;
}

char * 
nsInstallErrorMessages::GetString(int id, nsString* arg1) 
{
  char* msg=NULL;
  char* tag = XP_GetString(id);
  PR_ASSERT(tag != NULL);
  char* argMsg = "";
  
  msg = PL_strcat(msg, tag);
  
  if (arg1)
  {
      argMsg = arg1->ToNewCString();
      msg = PL_strcat(msg, argMsg);
      delete argMsg;
  }
  return msg;
}
