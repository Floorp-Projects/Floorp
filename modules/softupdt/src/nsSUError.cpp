/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "xp.h"
#include "xpgetstr.h"
#include "prprf.h"
#include "nsSUError.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


char * SU_GetErrorMsg1(int id, char* arg1) 
{
  char* errorMsg;
  char* tag = XP_GetString(id);
  PR_ASSERT(tag != NULL);
  errorMsg = XP_Cat(tag, arg1);
  return errorMsg;
}

char * SU_GetErrorMsg2(int id, nsString* arg1, int reason) 
{
  char* errorMsg;
  char* ptr;
  char* tag = XP_GetString(id);
  PR_ASSERT(tag != NULL);
  char* argMsg = "";
  if (argMsg)
    argMsg = arg1->ToNewCString();
  ptr = PR_sprintf_append(errorMsg, "%s %s %d", tag, argMsg, reason);
  delete argMsg;
  errorMsg = XP_STRDUP(ptr);
  delete ptr;
  return errorMsg;
}

char * SU_GetErrorMsg3(char *str, int err) 
{
  char* errorMsg;
  char* ptr;
  PR_ASSERT(str != NULL);
  ptr = PR_sprintf_append(errorMsg, "%s %d", str, err);
  errorMsg = XP_STRDUP(ptr);
  delete ptr;
  return errorMsg;
}

char * SU_GetErrorMsg4(int id, int reason) 
{
  char* msg;
  char* ptr;
  char* tag = XP_GetString(id);
  PR_ASSERT(tag != NULL);
  ptr = PR_sprintf_append(msg, "%s %d", tag, reason);
  msg = XP_STRDUP(ptr);
  delete ptr;
  return msg;
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
  if (arg1) {
    msg = XP_Cat(msg, tag, arg1);
  } else {
    msg = XP_Cat(msg, tag);
  }
  return msg;
}

char * SU_GetString2(int id, nsString* arg1) 
{
  char* msg=NULL;
  char* tag = XP_GetString(id);
  PR_ASSERT(tag != NULL);
  char* argMsg = "";
  if (arg1)
   argMsg = arg1->ToNewCString();
  msg = XP_Cat(msg, tag, argMsg);
  delete argMsg;
  return msg;
}


#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
