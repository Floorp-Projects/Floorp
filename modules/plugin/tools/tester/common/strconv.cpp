/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "xp.h"

#include "profile.h"
#include "strconv.h"

static char szDefaultString[] = PROFILE_DEFAULT_STRING;

static BOOL isTrueNumericValue(LPSTR szString)
{
  char szPrefix[] = PROFILE_NUMERIC_PREFIX;
  return (strncmp(szString, szPrefix, strlen(szPrefix)) == 0);
}

static LPSTR convertStringToLPSTR(LPSTR szString)
{
  if(szString == NULL)
    return NULL;

  if(isTrueNumericValue(szString))
  {
    char szPrefix[] = PROFILE_NUMERIC_PREFIX;
    return (LPSTR)atol(szString + strlen(szPrefix));
  }
  else
    return szString;
}

DWORD convertStringToLPSTR1(DWORD * pdw1)
{
  if(pdw1 == NULL)
    return 0L;

  DWORD dwRet = (isTrueNumericValue((LPSTR)(*pdw1)) ? (fTNV1) : 0);

  *pdw1 = (DWORD)convertStringToLPSTR((LPSTR)(*pdw1));

  return dwRet;
}

DWORD convertStringToLPSTR2(DWORD * pdw1, DWORD * pdw2)
{
  if((pdw1 == NULL) || (pdw2 == NULL))
    return 0L;

  DWORD dwRet =   (isTrueNumericValue((LPSTR)(*pdw1)) ? (fTNV1) : 0)
                | (isTrueNumericValue((LPSTR)(*pdw2)) ? (fTNV2) : 0);
  
  *pdw1 = (DWORD)convertStringToLPSTR((LPSTR)(*pdw1));
  *pdw2 = (DWORD)convertStringToLPSTR((LPSTR)(*pdw2));

  return dwRet;
}

DWORD convertStringToLPSTR3(DWORD * pdw1, DWORD * pdw2, DWORD * pdw3)
{
  if((pdw1 == NULL) || (pdw2 == NULL) || (pdw3 == NULL))
    return 0L;

  DWORD dwRet =   (isTrueNumericValue((LPSTR)(*pdw1)) ? (fTNV1) : 0)
                | (isTrueNumericValue((LPSTR)(*pdw2)) ? (fTNV2) : 0)
                | (isTrueNumericValue((LPSTR)(*pdw3)) ? (fTNV3) : 0);

  *pdw1 = (DWORD)convertStringToLPSTR((LPSTR)(*pdw1));
  *pdw2 = (DWORD)convertStringToLPSTR((LPSTR)(*pdw2));
  *pdw3 = (DWORD)convertStringToLPSTR((LPSTR)(*pdw3));

  return dwRet;
}

// converts numeric value represented by string to DWORD
static DWORD convertStringToDWORD(LPSTR szString)
{
  if(szString == NULL)
    return 0L;

  if(strcmp(szString, szDefaultString) == 0)
    return DEFAULT_DWARG_VALUE;
  else
    return (DWORD)atol(szString);
}

void convertStringToDWORD1(DWORD * pdw1)
{
  if(pdw1 == NULL)
    return;
  *pdw1 = convertStringToDWORD((LPSTR)(*pdw1));
}

void convertStringToDWORD2(DWORD * pdw1, DWORD * pdw2)
{
  if((pdw1 == NULL) || (pdw2 == NULL))
    return;
  *pdw1 = convertStringToDWORD((LPSTR)(*pdw1));
  *pdw2 = convertStringToDWORD((LPSTR)(*pdw2));
}

void convertStringToDWORD3(DWORD * pdw1, DWORD * pdw2, DWORD * pdw3)
{
  if((pdw1 == NULL) || (pdw2 == NULL) || (pdw3 == NULL))
    return;
  *pdw1 = convertStringToDWORD((LPSTR)(*pdw1));
  *pdw2 = convertStringToDWORD((LPSTR)(*pdw2));
  *pdw3 = convertStringToDWORD((LPSTR)(*pdw3));
}

void convertStringToDWORD4(DWORD * pdw1, DWORD * pdw2, DWORD * pdw3, DWORD * pdw4)
{
  if((pdw1 == NULL) || (pdw2 == NULL) || (pdw3 == NULL) || (pdw4 == NULL))
    return;

  *pdw1 = convertStringToDWORD((LPSTR)(*pdw1));
  *pdw2 = convertStringToDWORD((LPSTR)(*pdw2));
  *pdw3 = convertStringToDWORD((LPSTR)(*pdw3));
  *pdw4 = convertStringToDWORD((LPSTR)(*pdw4));
}

static NPBool convertStringToBOOL(LPSTR szString)
{
  if(szString == NULL)
    return (NPBool)NULL;

  if(isTrueNumericValue(szString))
  {
    char szPrefix[] = PROFILE_NUMERIC_PREFIX;
    return (NPBool)atol(szString + strlen(szPrefix));
  }
  else
  {
    NPBool npb = (strcmpi(szString, "TRUE") == 0) ? TRUE : FALSE;
    return npb;
  }
}

DWORD convertStringToBOOL1(DWORD * pdw1)
{
  if(pdw1 == NULL)
    return 0L;

  DWORD dwRet = (isTrueNumericValue((LPSTR)(*pdw1)) ? (fTNV1) : 0);

  *pdw1 = (DWORD)convertStringToBOOL((LPSTR)(*pdw1));

  return dwRet;
}

static NPReason convertStringToNPReason(LPSTR szString)
{
  if(szString == NULL)
    return NPRES_DONE;

  if(isTrueNumericValue(szString))
  {
    char szPrefix[] = PROFILE_NUMERIC_PREFIX;
    return (NPReason)atol(szString + strlen(szPrefix));
  }
  else
  {
    if(strcmpi(ENTRY_NPRES_DONE, szString) == 0)
      return NPRES_DONE;
    else if(strcmpi(ENTRY_NPRES_NETWORK_ERR, szString) == 0)
      return NPRES_NETWORK_ERR;
    else if(strcmpi(ENTRY_NPRES_USER_BREAK, szString) == 0)
      return NPRES_USER_BREAK;
    else
      return NPRES_DONE;
  }
}

DWORD convertStringToNPReason1(DWORD * pdw1)
{
  if(pdw1 == NULL)
    return 0L;

  *pdw1 = (DWORD)convertStringToNPReason((LPSTR)(*pdw1));

  return 0L;
}

static NPNVariable convertStringToNPNVariable(LPSTR szString)
{
  if(szString == NULL)
    return (NPNVariable)0;

  if(isTrueNumericValue(szString))
  {
    char szPrefix[] = PROFILE_NUMERIC_PREFIX;
    return (NPNVariable)atol(szString + strlen(szPrefix));
  }
  else
  {
    if(strcmpi(ENTRY_NPNVXDISPLAY, szString) == 0)
      return NPNVxDisplay;
    else if(strcmpi(ENTRY_NPNVXTAPPCONTEXT, szString) == 0)
      return NPNVxtAppContext;
    else if(strcmpi(ENTRY_NPNVNETSCAPEWINDOW, szString) == 0)
      return NPNVnetscapeWindow;
    else if(strcmpi(ENTRY_NPNVJAVASCRIPTENABLEDBOOL, szString) == 0)
      return NPNVjavascriptEnabledBool;
    else if(strcmpi(ENTRY_NPNVASDENABLEDBOOL, szString) == 0)
      return NPNVasdEnabledBool;
    else if(strcmpi(ENTRY_NPNVISOFFLINEBOOL, szString) == 0)
      return NPNVisOfflineBool;
    else
      return (NPNVariable)0;
  }
}

DWORD convertStringToNPNVariable1(DWORD * pdw1)
{
  if(pdw1 == NULL)
    return 0L;

  DWORD dwRet = (isTrueNumericValue((LPSTR)(*pdw1)) ? (fTNV1) : 0);
  
  *pdw1 = (DWORD)convertStringToNPNVariable((LPSTR)(*pdw1));

  return dwRet;
}

static NPPVariable convertStringToNPPVariable(LPSTR szString)
{
  if(szString == NULL)
    return (NPPVariable)0;

  if(isTrueNumericValue(szString))
  {
    char szPrefix[] = PROFILE_NUMERIC_PREFIX;
    return (NPPVariable)atol(szString + strlen(szPrefix));
  }
  else
  {
    if(strcmpi(ENTRY_NPPVPLUGINNAMESTRING, szString) == 0)
      return NPPVpluginNameString;
    else if(strcmpi(ENTRY_NPPVPLUGINDESCRIPTIONSTRING, szString) == 0)
      return NPPVpluginDescriptionString;
    else if(strcmpi(ENTRY_NPPVPLUGINWINDOWBOOL, szString) == 0)
      return NPPVpluginWindowBool;
    else if(strcmpi(ENTRY_NPPVPLUGINTRANSPARENTBOOL, szString) == 0)
      return NPPVpluginTransparentBool;
    else if(strcmpi(ENTRY_NPPVPLUGINWINDOWSIZE, szString) == 0)
      return NPPVpluginWindowSize;
    else
      return (NPPVariable)0;
  }
}

DWORD convertStringToNPPVariable1(DWORD * pdw1)
{
  if(pdw1 == NULL)
    return 0L;

  DWORD dwRet = (isTrueNumericValue((LPSTR)(*pdw1)) ? (fTNV1) : 0);

  *pdw1 = (DWORD)convertStringToNPPVariable((LPSTR)(*pdw1));

  return dwRet;
}
