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

#include "loghlp.h"

extern CLogger * pLogger;

LPSTR FormatNPAPIError(int iError)
{
  static char szError[64];
  switch (iError)
  {
    case NPERR_NO_ERROR:
      wsprintf(szError, "NPERR_NO_ERROR");
      break;
    case NPERR_GENERIC_ERROR:
      wsprintf(szError, "NPERR_GENERIC_ERROR");
      break;
    case NPERR_INVALID_INSTANCE_ERROR:
      wsprintf(szError, "NPERR_INVALID_INSTANCE_ERROR");
      break;
    case NPERR_INVALID_FUNCTABLE_ERROR:
      wsprintf(szError, "NPERR_INVALID_FUNCTABLE_ERROR");
      break;
    case NPERR_MODULE_LOAD_FAILED_ERROR:
      wsprintf(szError, "NPERR_MODULE_LOAD_FAILED_ERROR");
      break;
    case NPERR_OUT_OF_MEMORY_ERROR:
      wsprintf(szError, "NPERR_OUT_OF_MEMORY_ERROR");
      break;
    case NPERR_INVALID_PLUGIN_ERROR:
      wsprintf(szError, "NPERR_INVALID_PLUGIN_ERROR");
      break;
    case NPERR_INVALID_PLUGIN_DIR_ERROR:
      wsprintf(szError, "NPERR_INVALID_PLUGIN_DIR_ERROR");
      break;
    case NPERR_INCOMPATIBLE_VERSION_ERROR:
      wsprintf(szError, "NPERR_INCOMPATIBLE_VERSION_ERROR");
      break;
    case NPERR_INVALID_PARAM:
      wsprintf(szError, "NPERR_INVALID_PARAM");
      break;
    case NPERR_INVALID_URL:
      wsprintf(szError, "NPERR_INVALID_URL");
      break;
    case NPERR_FILE_NOT_FOUND:
      wsprintf(szError, "NPERR_FILE_NOT_FOUND");
      break;
    case NPERR_NO_DATA:
      wsprintf(szError, "NPERR_NO_DATA");
      break;
    case NPERR_STREAM_NOT_SEEKABLE:
      wsprintf(szError, "NPERR_STREAM_NOT_SEEKABLE");
      break;
    default:
      wsprintf(szError, "Unlisted error");
      break;
  }
  return &szError[0];
}

LPSTR FormatNPAPIReason(int iReason)
{
  static char szReason[64];
  switch (iReason)
  {
    case NPRES_DONE:
      wsprintf(szReason, "NPRES_DONE");
      break;
    case NPRES_NETWORK_ERR:
      wsprintf(szReason, "NPRES_NETWORK_ERR");
      break;
    case NPRES_USER_BREAK:
      wsprintf(szReason, "NPRES_USER_BREAK");
      break;
    default:
      wsprintf(szReason, "Unlisted reason");
      break;
  }
  return &szReason[0];
}

LPSTR FormatNPNVariable(NPNVariable var)
{
  static char szVar[64];
  switch (var)
  {
    case NPNVxDisplay:
      wsprintf(szVar, "NPNVxDisplay");
      break;
    case NPNVxtAppContext:
      wsprintf(szVar, "NPNVxtAppContext");
      break;
    case NPNVnetscapeWindow:
      wsprintf(szVar, "NPNVnetscapeWindow");
      break;
    case NPNVjavascriptEnabledBool:
      wsprintf(szVar, "NPNVjavascriptEnabledBool");
      break;
    case NPNVasdEnabledBool:
      wsprintf(szVar, "NPNVasdEnabledBool");
      break;
    case NPNVisOfflineBool:
      wsprintf(szVar, "NPNVisOfflineBool");
      break;
    default:
      wsprintf(szVar, "Unlisted variable");
      break;
  }
  return &szVar[0];
}

LPSTR FormatNPPVariable(NPPVariable var)
{
  static char szVar[64];
  switch (var)
  {
    case NPPVpluginNameString:
      wsprintf(szVar, "NPPVpluginNameString");
      break;
    case NPPVpluginDescriptionString:
      wsprintf(szVar, "NPPVpluginDescriptionString");
      break;
    case NPPVpluginWindowBool:
      wsprintf(szVar, "NPPVpluginWindowBool");
      break;
    case NPPVpluginTransparentBool:
      wsprintf(szVar, "NPPVpluginTransparentBool");
      break;
    case NPPVpluginWindowSize:
      wsprintf(szVar, "NPPVpluginWindowSize");
      break;
    default:
      wsprintf(szVar, "Unlisted variable");
      break;
  }
  return &szVar[0];
}

BOOL FormatPCHARArgument(LPSTR szBuf, int iLength, LogArgumentStruct * parg)
{
  if(iLength <= parg->iLength)
    return FALSE;

  if(parg->pData == NULL)
    wsprintf(szBuf, "%#08lx", parg->dwArg);
  else
    wsprintf(szBuf, "%#08lx(\"%s\")", parg->dwArg, (LPSTR)parg->pData);
  return TRUE;
}

BOOL FormatBOOLArgument(LPSTR szBuf, int iLength, LogArgumentStruct * parg)
{
  if(iLength <= 8)
    return FALSE;

  wsprintf(szBuf, "%s", ((NPBool)parg->dwArg == TRUE) ? "TRUE" : "FALSE");
  return TRUE;
}

BOOL FormatPBOOLArgument(LPSTR szBuf, int iLength, LogArgumentStruct * parg)
{
  if(iLength <= 8)
    return FALSE;

  wsprintf(szBuf, "%#08lx(%s)", parg->dwArg, (*((NPBool *)parg->pData) == TRUE) ? "TRUE" : "FALSE");
  return TRUE;
}

static void makeAbbreviatedString(LPSTR szBuf, int iSize, DWORD dwArg, int iLength, int iWrap)
{
  if(dwArg == 0L)
  {
    szBuf[0] = '\0';
    return;
  }

  if(iLength > iWrap)
  {
    int iRealWrap = (iSize > iWrap) ? iWrap : (iSize - 4);
    memcpy((LPVOID)&szBuf[0], (LPVOID)dwArg, iRealWrap);
    szBuf[iRealWrap]     = '.';
    szBuf[iRealWrap + 1] = '.';
    szBuf[iRealWrap + 2] = '.';
    szBuf[iRealWrap + 3] = '\0';
  }
  else
  {
    if(iLength >= iSize)
    {    
      memcpy((LPVOID)&szBuf[0], (LPVOID)dwArg, iSize - 4);
      szBuf[iSize]     = '.';
      szBuf[iSize + 1] = '.';
      szBuf[iSize + 2] = '.';
      szBuf[iSize + 3] = '\0';
    }
    else
    {
      memcpy((LPVOID)&szBuf[0], (LPVOID)dwArg, iLength);
      szBuf[iLength] = '\0';
    }
  }
}

LogItemStruct * makeLogItemStruct(NPAPI_Action action, 
                                  DWORD dwTimeEnter, DWORD dwTimeReturn, 
                                  DWORD dwRet, 
                                  DWORD dw1, DWORD dw2, DWORD dw3, DWORD dw4, 
                                  DWORD dw5, DWORD dw6, DWORD dw7, BOOL bShort)
{
  int iWrap = pLogger->getStringDataWrap();

  LogItemStruct * plis = new LogItemStruct;
  if(plis == NULL)
    return NULL;

  plis->dwTimeEnter = dwTimeEnter;
  plis->dwTimeReturn = dwTimeReturn;
  plis->action = action;
  plis->dwReturnValue = dwRet;
  plis->arg1.dwArg = dw1;
  plis->arg2.dwArg = dw2;
  plis->arg3.dwArg = dw3;
  plis->arg4.dwArg = dw4;
  plis->arg5.dwArg = dw5;
  plis->arg6.dwArg = dw6;
  plis->arg7.dwArg = dw7;

  char szTarget[1024] = {'\0'};
  char szBuf[1024] = {'\0'};

  if(bShort)
    return plis;

  switch (action)
  {
    case action_invalid:
      break;

    // NPN action
    case action_npn_version:
      plis->arg1.pData = new int[1];
      *(int*)(plis->arg1.pData) = *((int*)dw1);
      plis->arg1.iLength = sizeof(int);

      plis->arg2.pData = new int[1];
      *(int*)(plis->arg2.pData) = *((int*)dw2);
      plis->arg2.iLength = sizeof(int);

      plis->arg3.pData = new int[1];
      *(int*)(plis->arg3.pData) = *((int*)dw3);
      plis->arg3.iLength = sizeof(int);

      plis->arg4.pData = new int[1];
      *(int*)(plis->arg4.pData) = *((int*)dw4);
      plis->arg4.iLength = sizeof(int);

      break;
    case action_npn_get_url_notify:
      if(dw2 != 0L)
      {
        plis->arg2.iLength = strlen((LPSTR)dw2) + 1;
        plis->arg2.pData = new char[plis->arg2.iLength];
        memcpy(plis->arg2.pData, (LPVOID)dw2, plis->arg2.iLength);
      }

      if(dw3 != 0L)
      {
        makeAbbreviatedString(szTarget, sizeof(szTarget), dw3, strlen((LPSTR)dw3), iWrap);
        plis->arg3.iLength = strlen(szTarget) + 1;
        plis->arg3.pData = new char[plis->arg3.iLength];
        memcpy(plis->arg3.pData, (LPVOID)&szTarget[0], plis->arg3.iLength);
      }
      break;
    case action_npn_get_url:
    {
      if(dw2 != 0L)
      {
        plis->arg2.iLength = strlen((LPSTR)dw2) + 1;
        plis->arg2.pData = new char[plis->arg2.iLength];
        memcpy(plis->arg2.pData, (LPVOID)dw2, plis->arg2.iLength);
      }

      if(dw3 != 0L)
      {
        makeAbbreviatedString(szTarget, sizeof(szTarget), dw3, strlen((LPSTR)dw3), iWrap);
        plis->arg3.iLength = strlen(szTarget) + 1;
        plis->arg3.pData = new char[plis->arg3.iLength];
        memcpy(plis->arg3.pData, (LPVOID)&szTarget[0], plis->arg3.iLength);
      }
      break;
    }
    case action_npn_post_url_notify:
    {
      if(dw2 != 0L)
      {
        plis->arg2.iLength = strlen((LPSTR)dw2) + 1;
        plis->arg2.pData = new char[plis->arg2.iLength];
        memcpy(plis->arg2.pData, (LPVOID)dw2, plis->arg2.iLength);
      }

      if(dw3 != 0L)
      {
        makeAbbreviatedString(szTarget, sizeof(szTarget), dw3, strlen((LPSTR)dw3), iWrap);
        plis->arg3.iLength = strlen(szTarget) + 1;
        plis->arg3.pData = new char[plis->arg3.iLength];
        memcpy(plis->arg3.pData, (LPVOID)&szTarget[0], plis->arg3.iLength);
      }

      makeAbbreviatedString(szBuf, sizeof(szBuf), dw5, strlen((LPSTR)dw5), iWrap);
      plis->arg5.iLength = (int)dw4 + 1;
      plis->arg5.pData = new char[plis->arg5.iLength];
      memcpy(plis->arg5.pData, (LPVOID)&szBuf[0], plis->arg5.iLength);

      break;
    }
    case action_npn_post_url:
    {
      if(dw2 != 0L)
      {
        plis->arg2.iLength = strlen((LPSTR)dw2) + 1;
        plis->arg2.pData = new char[plis->arg2.iLength];
        memcpy(plis->arg2.pData, (LPVOID)dw2, plis->arg2.iLength);
      }

      if(dw3 != 0L)
      {
        makeAbbreviatedString(szTarget, sizeof(szTarget), dw3, strlen((LPSTR)dw3), iWrap);
        plis->arg3.iLength = strlen(szTarget) + 1;
        plis->arg3.pData = new char[plis->arg3.iLength];
        memcpy(plis->arg3.pData, (LPVOID)&szTarget[0], plis->arg3.iLength);
      }

      makeAbbreviatedString(szBuf, sizeof(szBuf), dw5, strlen((LPSTR)dw5), iWrap);
      plis->arg5.iLength = (int)dw4 + 1;
      plis->arg5.pData = new char[plis->arg5.iLength];
      memcpy(plis->arg5.pData, (LPVOID)&szBuf[0], plis->arg5.iLength);

      break;
    }
    case action_npn_new_stream:
    {
      if(dw2 != 0L)
      {
        plis->arg2.iLength = strlen((LPSTR)dw2) + 1;
        plis->arg2.pData = new char[plis->arg2.iLength];
        memcpy(plis->arg2.pData, (LPVOID)dw2, plis->arg2.iLength);
      }

      makeAbbreviatedString(szTarget, sizeof(szTarget), dw3, strlen((LPSTR)dw3), iWrap);
      plis->arg3.iLength = strlen(szTarget) + 1;
      plis->arg3.pData = new char[plis->arg3.iLength];
      memcpy(plis->arg3.pData, (LPVOID)&szTarget[0], plis->arg3.iLength);

      plis->arg4.pData = new char[sizeof(DWORD)];
      plis->arg4.iLength = sizeof(DWORD);
      memcpy(plis->arg4.pData, (LPVOID)dw4, plis->arg4.iLength);

      break;
    }
    case action_npn_destroy_stream:
      break;
    case action_npn_request_read:
      break;
    case action_npn_write:
    {
      makeAbbreviatedString(szBuf, sizeof(szBuf), dw4, strlen((LPSTR)dw4), iWrap);
      plis->arg4.iLength = strlen(szBuf) + 1;
      plis->arg4.pData = new char[plis->arg4.iLength];
      memcpy(plis->arg4.pData, (LPVOID)&szBuf[0], plis->arg4.iLength);
      break;
    }
    case action_npn_status:
      if(dw2 != 0L)
      {
        plis->arg2.iLength = strlen((LPSTR)dw2) + 1;
        plis->arg2.pData = new char[plis->arg2.iLength];
        memcpy(plis->arg2.pData, (LPVOID)dw2, plis->arg2.iLength);
      }
      break;
    case action_npn_user_agent:
      break;
    case action_npn_mem_alloc:
      break;
    case action_npn_mem_free:
      break;
    case action_npn_mem_flush:
      break;
    case action_npn_reload_plugins:
      break;
    case action_npn_get_java_env:
      break;
    case action_npn_get_java_peer:
      break;
    case action_npn_get_value:
      plis->arg3.iLength = sizeof(DWORD);
      plis->arg3.pData = new char[plis->arg3.iLength];
      memcpy(plis->arg3.pData, (LPVOID)dw3, plis->arg3.iLength);
      break;
    case action_npn_set_value:
      if(((NPPVariable)dw2 == NPPVpluginNameString) || ((NPPVariable)dw2 == NPPVpluginDescriptionString))
      {
        makeAbbreviatedString(szBuf, sizeof(szBuf), dw3, strlen((char *)dw3), iWrap);
        plis->arg3.iLength = strlen(szBuf) + 1;
        plis->arg3.pData = new char[plis->arg3.iLength];
        memcpy(plis->arg3.pData, (LPVOID)&szBuf[0], plis->arg3.iLength);
      }
      else if(((NPPVariable)dw2 == NPPVpluginWindowBool) || ((NPPVariable)dw2 == NPPVpluginTransparentBool))
      {
        plis->arg3.iLength = sizeof(NPBool);
        plis->arg3.pData = new char[plis->arg3.iLength];
        memcpy(plis->arg3.pData, (LPVOID)dw3, plis->arg3.iLength);
      }
      else if((NPPVariable)dw2 == NPPVpluginWindowSize)
      {
        plis->arg3.iLength = sizeof(NPSize);
        plis->arg3.pData = new char[plis->arg3.iLength];
        memcpy(plis->arg3.pData, (LPVOID)dw3, plis->arg3.iLength);
      }
      break;
    case action_npn_invalidate_rect:
    {
      plis->arg2.iLength = sizeof(NPRect);
      plis->arg2.pData = new char[plis->arg2.iLength];
      memcpy(plis->arg2.pData, (LPVOID)dw2, plis->arg2.iLength);
      break;
    }
    case action_npn_invalidate_region:
      break;
    case action_npn_force_redraw:
      break;

    // NPP action
    case action_np_initialize:
      break;
    case action_np_shutdown:
      break;
    case action_npp_new:
      plis->arg1.iLength = strlen((LPSTR)dw1) + 1;
      plis->arg1.pData = new char[plis->arg1.iLength];
      memcpy(plis->arg1.pData, (LPVOID)dw1, plis->arg1.iLength);
      break;
    case action_npp_destroy:
      plis->arg2.iLength = sizeof(DWORD);
      plis->arg2.pData = new char[plis->arg2.iLength];
      memcpy(plis->arg2.pData, (LPVOID)dw2, plis->arg2.iLength);
      break;
    case action_npp_set_window:
      plis->arg2.iLength = sizeof(NPWindow);
      plis->arg2.pData = new char[plis->arg2.iLength];
      memcpy(plis->arg2.pData, (LPVOID)dw2, plis->arg2.iLength);
      break;
    case action_npp_new_stream:
      plis->arg2.iLength = strlen((LPSTR)dw2) + 1;
      plis->arg2.pData = new char[plis->arg2.iLength];
      memcpy(plis->arg2.pData, (LPVOID)dw2, plis->arg2.iLength);

      plis->arg5.iLength = sizeof(uint16);
      plis->arg5.pData = new char[plis->arg5.iLength];
      memcpy(plis->arg5.pData, (LPVOID)dw5, plis->arg5.iLength);
      break;
    case action_npp_destroy_stream:
      break;
    case action_npp_stream_as_file:
      plis->arg3.iLength = strlen((LPSTR)dw3) + 1;
      plis->arg3.pData = new char[plis->arg3.iLength];
      memcpy(plis->arg3.pData, (LPVOID)dw3, plis->arg3.iLength);
      break;
    case action_npp_write_ready:
      break;
    case action_npp_write:
    {
      if(dw5 != 0L)
      {
        makeAbbreviatedString(szBuf, sizeof(szBuf), dw5, strlen((LPSTR)dw5), iWrap);
        plis->arg5.iLength = strlen(szBuf) + 1;
        plis->arg5.pData = new char[plis->arg5.iLength];
        memcpy(plis->arg5.pData, (LPVOID)&szBuf[0], plis->arg5.iLength);
      }
      break;
    }
    case action_npp_print:
      break;
    case action_npp_handle_event:
      break;
    case action_npp_url_notify:
      plis->arg2.iLength = strlen((LPSTR)dw2) + 1;
      plis->arg2.pData = new char[plis->arg2.iLength];
      memcpy(plis->arg2.pData, (LPVOID)dw2, plis->arg2.iLength);
      break;
    case action_npp_get_java_class:
      break;
    case action_np_get_mime_description:
      break;
    case action_npp_get_value:
      break;
    case action_npp_set_value:
      break;

    default:
      break;
  }

  return plis;
}

int formatLogItem(LogItemStruct * plis, LPSTR szOutput, LPSTR szEndOfItem, BOOL bDOSStyle)
{
  int iRet = 0;
  static char szTime[128];
  static char szString[1024];
  static char szEOL[8];
  static char szEOI[256];
  static char szRet[256];

  if(bDOSStyle)
  {
    strcpy(szEOL, "\r\n");
    strcpy(szEOI, szEndOfItem);
    strcat(szEOI, "\r\n");
  }
  else
  {
    strcpy(szEOL, "\n");
    strcpy(szEOI, szEndOfItem);
    strcat(szEOI, "\n");
  }

  szRet[0] = '\0';
  szOutput[0] = '\0';

  wsprintf(szTime, "Time: %lu %lu (%li)%s", 
                   plis->dwTimeEnter, plis->dwTimeReturn, 
                   (long)plis->dwTimeReturn - (long)plis->dwTimeEnter, szEOL);

  DWORD dw1 = plis->arg1.dwArg;
  DWORD dw2 = plis->arg2.dwArg;
  DWORD dw3 = plis->arg3.dwArg;
  DWORD dw4 = plis->arg4.dwArg;
  DWORD dw5 = plis->arg5.dwArg;
  DWORD dw6 = plis->arg6.dwArg;
  DWORD dw7 = plis->arg7.dwArg;

  char sz1[1024] = {'\0'};
  char sz2[1024] = {'\0'};
  char sz3[1024] = {'\0'};
  char sz4[1024] = {'\0'};
  char sz5[1024] = {'\0'};
  char sz6[1024] = {'\0'};

  switch (plis->action)
  {
    case action_invalid:
      break;

    // NPN action
    case action_npn_version:
      if((plis->arg1.pData != NULL)&&(plis->arg2.pData != NULL)&&(plis->arg3.pData != NULL)&&(plis->arg4.pData != NULL))
      {
        wsprintf(szRet, "Plugin %i.%i, Netscape %i.%i%s", 
                 *(int*)plis->arg1.pData, *(int*)plis->arg2.pData,
                 *(int*)plis->arg3.pData, *(int*)plis->arg4.pData,szEOL);
        wsprintf(szString, "NPN_Version(%#08lx, %#08lx, %#08lx, %#08lx)%s%s", dw1,dw2,dw3,dw4,szEOL,szRet);
      }
      else
        wsprintf(szString, "NPN_Version(%#08lx, %#08lx, %#08lx, %#08lx)%s", dw1,dw2,dw3,dw4,szEOL);
      break;
    case action_npn_get_url_notify:
    {
      FormatPCHARArgument(sz2, sizeof(sz2), &plis->arg2);
      FormatPCHARArgument(sz3, sizeof(sz3), &plis->arg3);
      wsprintf(szRet, "Returns: %s%s", FormatNPAPIError((int)plis->dwReturnValue),szEOL);
      wsprintf(szString, "NPN_GetURLNotify(%#08lx, %s, %s, %#08lx)%s%s", dw1,sz2,sz3,dw4,szEOL,szRet);
      break;
    }
    case action_npn_get_url:
    {
      FormatPCHARArgument(sz2, sizeof(sz2), &plis->arg2);
      FormatPCHARArgument(sz3, sizeof(sz3), &plis->arg3);
      wsprintf(szRet, "Returns: %s%s", FormatNPAPIError((int)plis->dwReturnValue),szEOL);
      wsprintf(szString, "NPN_GetURL(%#08lx, %s, %s)%s%s", dw1,sz2,sz3,szEOL,szRet);
      break;
    }
    case action_npn_post_url_notify:
    {
      FormatPCHARArgument(sz2, sizeof(sz2), &plis->arg2);
      FormatPCHARArgument(sz3, sizeof(sz3), &plis->arg3);
      FormatPCHARArgument(sz5, sizeof(sz5), &plis->arg5);
      FormatBOOLArgument(sz6, sizeof(sz6), &plis->arg6);

      wsprintf(szRet, "Returns: %s%s", FormatNPAPIError((int)plis->dwReturnValue),szEOL);
      wsprintf(szString, "NPN_PostURLNotify(%#08lx, %s, %s, %li, %s, %s, %#08lx)%s%s", 
               dw1,sz2,sz3,(uint32)dw4,sz5,sz6,dw7,szEOL,szRet);
      break;
    }
    case action_npn_post_url:
    {
      FormatPCHARArgument(sz2, sizeof(sz2), &plis->arg2);
      FormatPCHARArgument(sz3, sizeof(sz3), &plis->arg3);
      FormatPCHARArgument(sz5, sizeof(sz5), &plis->arg5);
      FormatBOOLArgument(sz6, sizeof(sz6), &plis->arg6);

      wsprintf(szRet, "Returns: %s%s", FormatNPAPIError((int)plis->dwReturnValue),szEOL);
      wsprintf(szString, "NPN_PostURL(%#08lx, %s, %s, %li, %s, %s)%s%s", 
               dw1,sz2,sz3,(uint32)dw4,sz5,sz6,szEOL,szRet);
      break;
    }
    case action_npn_new_stream:
    {
      FormatPCHARArgument(sz2, sizeof(sz2), &plis->arg2);
      FormatPCHARArgument(sz3, sizeof(sz3), &plis->arg3);
      wsprintf(szRet, "Returns: %s%s", FormatNPAPIError((int)plis->dwReturnValue),szEOL);
      if(plis->arg4.pData != NULL)
        wsprintf(szString, "NPN_NewStream(%#08lx, %s, %s, %#08lx(%#08lx))%s%s", 
                 dw1, sz2,sz3,dw4,*(DWORD *)plis->arg4.pData,szEOL,szRet);
      else
        wsprintf(szString, "NPN_NewStream(%#08lx, \"%s\", \"%s\", %#08lx)%s%s", dw1, sz2,sz3,dw4,szEOL,szRet);
      break;
    }
    case action_npn_destroy_stream:
      wsprintf(szRet, "Returns: %s%s", FormatNPAPIError((int)plis->dwReturnValue),szEOL);
      wsprintf(szString, "NPN_DestroyStream(%#08lx, %#08lx, %s)%s%s", dw1,dw2,FormatNPAPIReason((int)dw3),szEOL,szRet);
      break;
    case action_npn_request_read:
      wsprintf(szRet, "Returns: %s%s", FormatNPAPIError((int)plis->dwReturnValue),szEOL);
      wsprintf(szString, "NPN_RequestRead(%#08lx, %#08lx)%s%s", dw1, dw2, szEOL, szRet);
      break;
    case action_npn_write:
    {
      FormatPCHARArgument(sz4, sizeof(sz4), &plis->arg4);
      wsprintf(szRet, "Returns: %li%s", plis->dwReturnValue,szEOL);
      wsprintf(szString, "NPN_Write(%#08lx, %#08lx, %li, %s)%s%s", dw1, dw2, (int32)dw3, sz4, szEOL, szRet);
      break;
    }
    case action_npn_status:
    {
      FormatPCHARArgument(sz2, sizeof(sz2), &plis->arg2);
      wsprintf(szString, "NPN_Status(%#08lx, %s)%s", dw1, sz2, szEOL);
      break;
    }
    case action_npn_user_agent:
      wsprintf(szRet, "Returns: %#08lx(\"%s\")%s", plis->dwReturnValue, (LPSTR)plis->dwReturnValue,szEOL);
      wsprintf(szString, "NPN_UserAgent(%#08lx)%s%s", dw1, szEOL, szRet);
      break;
    case action_npn_mem_alloc:
      wsprintf(szRet, "Returns: %#08lx%s", plis->dwReturnValue, szEOL);
      wsprintf(szString, "NPN_MemAlloc(%li)%s%s", dw1, szEOL, szRet);
      break;
    case action_npn_mem_free:
      wsprintf(szString, "NPN_MemFree(%#08lx)%s", dw1,szEOL);
      break;
    case action_npn_mem_flush:
      wsprintf(szRet, "Returns: %li%s", plis->dwReturnValue,szEOL);
      wsprintf(szString, "NPN_MemFlush(%li)%s%s", dw1, szEOL, szRet);
      break;
    case action_npn_reload_plugins:
    {
      FormatBOOLArgument(sz1, sizeof(sz1), &plis->arg1);
      wsprintf(szString, "NPN_ReloadPlugins(%s)%s", sz1,szEOL);
      break;
    }
    case action_npn_get_java_env:
      wsprintf(szRet, "Returns: %#08lx%s", plis->dwReturnValue,szEOL);
      wsprintf(szString, "NPN_GetJavaEnv()%s%s", szEOL, szRet);
      break;
    case action_npn_get_java_peer:
      wsprintf(szRet, "Returns: %#08lx%s", plis->dwReturnValue,szEOL);
      wsprintf(szString, "NPN_GetJavaPeer(%#08lx)%s%s", dw1, szEOL, szRet);
      break;
    case action_npn_get_value:
    {
      wsprintf(szRet, "Returns: %s%s", FormatNPAPIError((int)plis->dwReturnValue),szEOL);
      switch(dw2)
      {
        case NPNVxDisplay:
        case NPNVxtAppContext:
        case NPNVnetscapeWindow:
          if(dw3 != 0L)
            wsprintf(szString, "NPN_GetValue(%#08lx, %s, %#08lx(%#08lx))%s%s",dw1,FormatNPNVariable((NPNVariable)dw2),dw3,*(DWORD *)dw3,szEOL,szRet);
          else
            wsprintf(szString, "NPN_GetValue(%#08lx, %s, %#08lx)%s%s",dw1,FormatNPNVariable((NPNVariable)dw2),dw3,szEOL,szRet);
          break;
        case NPNVjavascriptEnabledBool:
        case NPNVasdEnabledBool:
        case NPNVisOfflineBool:
          if(dw3 != 0L)
            wsprintf(szString, "NPN_GetValue(%#08lx, %s, %#08lx(%s))%s%s",
                     dw1,FormatNPNVariable((NPNVariable)dw2),dw3,
                     (((NPBool)*(DWORD *)dw3) == TRUE) ? "TRUE" : "FALSE", szEOL, szRet);
          else
            wsprintf(szString, "NPN_GetValue(%#08lx, %s, %#08lx)%s%s",dw1,FormatNPNVariable((NPNVariable)dw2),dw3,szEOL,szRet);
          break;
        default:
          wsprintf(szRet, "Returns: %s%s", FormatNPAPIError((int)plis->dwReturnValue),szEOL);
          break;
      }
      break;
    }
    case action_npn_set_value:
      wsprintf(szRet, "Returns: %s%s", FormatNPAPIError((int)plis->dwReturnValue),szEOL);

      if(((NPPVariable)dw2 == NPPVpluginNameString) || ((NPPVariable)dw2 == NPPVpluginDescriptionString))
      {
        FormatPCHARArgument(sz3, sizeof(sz3), &plis->arg3);
        wsprintf(szString, "NPN_SetValue(%#08lx, %s, %s)%s%s", dw1,FormatNPPVariable((NPPVariable)dw2),sz3,szEOL,szRet);
      }
      else if(((NPPVariable)dw2 == NPPVpluginWindowBool) || ((NPPVariable)dw2 == NPPVpluginTransparentBool))
      {
        FormatPBOOLArgument(sz3, sizeof(sz3), &plis->arg3);
        wsprintf(szString, "NPN_SetValue(%#08lx, %s, %s)%s%s", 
                 dw1,FormatNPPVariable((NPPVariable)dw2),sz3,szEOL,szRet);
      }
      else if((NPPVariable)dw2 == NPPVpluginWindowSize)
      {
        if(plis->arg3.pData != NULL)
        {
          int32 iWidth = ((NPSize *)plis->arg3.pData)->width;
          int32 iHeight = ((NPSize *)plis->arg3.pData)->height;
          wsprintf(szString, "NPN_SetValue(%#08lx, %s, %#08lx(%li,%li))%s%s", 
                   dw1,FormatNPPVariable((NPPVariable)dw2),dw3,iWidth,iHeight,szEOL,szRet);
        }
        else
          wsprintf(szString, "NPN_SetValue(%#08lx, %s, %#08lx(?,?))%s%s", 
                   dw1,FormatNPPVariable((NPPVariable)dw2),dw3,szEOL,szRet);
      }
      else
        wsprintf(szString, "NPN_SetValue(%#08lx, %s, %#08lx(What is it?))%s%s", dw1,FormatNPPVariable((NPPVariable)dw2),dw3,szEOL,szRet);
      break;
    case action_npn_invalidate_rect:
    {
      if(plis->arg2.pData != NULL)
      {
        uint16 top    = ((NPRect *)plis->arg2.pData)->top;
        uint16 left   = ((NPRect *)plis->arg2.pData)->left;
        uint16 bottom = ((NPRect *)plis->arg2.pData)->bottom;
        uint16 right  = ((NPRect *)plis->arg2.pData)->right;
        wsprintf(szString, "NPN_InvalidateRect(%#08lx, %#08lx(%u,%u;%u,%u)%s", dw1,dw2,top,left,bottom,right,szEOL);
      }
      else
        wsprintf(szString, "NPN_InvalidateRect(%#08lx, %#08lx(?,?,?,?)%s", dw1,dw2,szEOL);
      break;
    }
    case action_npn_invalidate_region:
      wsprintf(szString, "NPN_InvalidateRegion(%#08lx, %#08lx)%s", dw1,dw2,szEOL);
      break;
    case action_npn_force_redraw:
      wsprintf(szString, "NPN_ForceRedraw(%#08lx)%s", dw1,szEOL);
      break;

    // NPP action
    case action_np_initialize:
      wsprintf(szRet, "Returning: %s%s", FormatNPAPIError((int)plis->dwReturnValue),szEOL);
      wsprintf(szString, "Global initialization%s%s", szEOL, szRet);
      break;
    case action_np_shutdown:
      wsprintf(szString, "Global shutdown%s",szEOL);
      break;
    case action_npp_new:
    {
      wsprintf(szRet, "Returning: %s%s", FormatNPAPIError((int)plis->dwReturnValue),szEOL);
      char szMode[16];
      switch (dw3)
      {
        case NP_EMBED:
          strcpy(szMode, "NP_EMBED");
          break;
        case NP_FULL:
          strcpy(szMode, "NP_FULL");
          break;
        default:
          strcpy(szMode, "[Invalide mode]");
          break;
      }
      wsprintf(szString, "NPP_New(\"%s\", %#08lx, %s, %i, %#08lx, %#08lx, %#08lx)%s%s", 
               (LPSTR)dw1,dw2,szMode,(int)dw4,dw5,dw6,dw7,szEOL,szRet);
      break;
    }
    case action_npp_destroy:
      wsprintf(szRet, "Returning: %s%s", FormatNPAPIError((int)plis->dwReturnValue),szEOL);
      wsprintf(szString, "NPP_Destroy(%#08lx, %#08lx(%#08lx))%s%s", dw1, dw2, *(DWORD *)plis->arg2.pData,szEOL,szRet);
      break;
    case action_npp_set_window:
    {
      wsprintf(szRet, "Returning: %s%s", FormatNPAPIError((int)plis->dwReturnValue),szEOL);
      char szWindow[512];

      if(plis->arg2.pData != NULL)
      {
        char szType[80];
        switch (((NPWindow*)plis->arg2.pData)->type)
        {
          case NPWindowTypeWindow:
            wsprintf(szType, "NPWindowTypeWindow");
            break;
          case NPWindowTypeDrawable:
            wsprintf(szType, "NPWindowTypeDrawable");
            break;
          default:
            wsprintf(szType, "[Unlisted type]");
            break;
        }
        wsprintf(szWindow, "NPWindow: %#08lx, (%li,%li), (%li,%li), (%i,%i,%i,%i), %s", 
                 ((NPWindow*)plis->arg2.pData)->window, 
                 ((NPWindow*)plis->arg2.pData)->x, 
                 ((NPWindow*)plis->arg2.pData)->y, 
                 ((NPWindow*)plis->arg2.pData)->width, 
                 ((NPWindow*)plis->arg2.pData)->height, 
                 ((NPWindow*)plis->arg2.pData)->clipRect.top, 
                 ((NPWindow*)plis->arg2.pData)->clipRect.left, 
                 ((NPWindow*)plis->arg2.pData)->clipRect.bottom, 
                 ((NPWindow*)plis->arg2.pData)->clipRect.right, szType);
        wsprintf(szString, "NPP_SetWindow(%#08lx, %#08lx)%s%s%s%s", dw1,dw2,szEOL,szWindow,szEOL,szRet);
      }
      else
        wsprintf(szString, "NPP_SetWindow(%#08lx, %#08lx)%s%s", dw1,dw2,szEOL,szRet);

      break;
    }
    case action_npp_new_stream:
    {
      switch (*(int16 *)plis->arg5.pData)
      {
        case NP_NORMAL:
          wsprintf(sz5, "NP_NORMAL");
          break;
        case NP_ASFILEONLY:
          wsprintf(sz5, "NP_ASFILEONLY");
          break;
        case NP_ASFILE:
          wsprintf(sz5, "NP_ASFILE");
          break;
        default:
          wsprintf(sz5, "[Unlisted type]");
          break;
      }
      wsprintf(szRet, "Returning: %s%s", FormatNPAPIError((int)plis->dwReturnValue),szEOL);
      FormatPCHARArgument(sz2, sizeof(sz2), &plis->arg2);
      wsprintf(szString, "NPP_NewStream(%#08lx, %s, %#08lx, %s, %s)%s%s", dw1, sz2, dw3, 
               ((NPBool)dw4 == TRUE) ? "TRUE" : "FALSE", sz5, szEOL, szRet);
      break;
    }
    case action_npp_destroy_stream:
      wsprintf(szRet, "Returning: %s%s", FormatNPAPIError((int)plis->dwReturnValue),szEOL);
      wsprintf(szString, "NPP_DestroyStream(%#08lx, %#08lx, %s)%s%s", dw1,dw2,FormatNPAPIReason((int)dw3),szEOL,szRet);
      break;
    case action_npp_stream_as_file:
      FormatPCHARArgument(sz3, sizeof(sz3), &plis->arg3);
      wsprintf(szString, "NPP_StreamAsFile(%#08lx, %#08lx, %s)%s", dw1,dw2,sz3,szEOL);
      break;
    case action_npp_write_ready:
      wsprintf(szRet, "Returning: %li%s", plis->dwReturnValue,szEOL);
      wsprintf(szString, "NPP_WriteReady(%#08lx, %#08lx)%s%s", dw1,dw2,szEOL,szRet);
      break;
    case action_npp_write:
    {
      wsprintf(szRet, "Returning: %li%s", plis->dwReturnValue,szEOL);
      FormatPCHARArgument(sz5, sizeof(sz5), &plis->arg5);
      wsprintf(szString, "NPP_Write(%#08lx, %#08lx, %li, %li, %s))%s%s",dw1,dw2,dw3,dw4,sz5,szEOL,szRet);
      break;
    }
    case action_npp_print:
      wsprintf(szString, "NPP_Print(%#08lx, %#08lx)%s", dw1, dw2,szEOL);
      break;
    case action_npp_handle_event:
      wsprintf(szRet, "Returning: %i%s", (int16)plis->dwReturnValue,szEOL);
      wsprintf(szString, "NPP_HandleEvent(%#08lx, %#08lx)%s%s", dw1,dw2,szEOL,szRet);
      break;
    case action_npp_url_notify:
    {
      FormatPCHARArgument(sz2, sizeof(sz2), &plis->arg2);
      wsprintf(szString, "NPP_URLNotify(%#08lx, %s, %s, %#08lx)%s", dw1,sz2,FormatNPAPIReason((int)dw3),dw4,szEOL);
      break;
    }
    case action_npp_get_java_class:
      wsprintf(szRet, "Returning: %#08lx%s", plis->dwReturnValue,szEOL);
      wsprintf(szString, "NPP_GetJavaClass()%s%s",szEOL,szRet);
      break;
    case action_np_get_mime_description:
      wsprintf(szRet, "Returning: %#08lx%s", plis->dwReturnValue,szEOL);
      wsprintf(szString, "NPP_GetMIMEDescription()%s%s",szEOL,szRet);
      break;
    case action_npp_get_value:
      wsprintf(szRet, "Returning: %s%s", FormatNPAPIError((int)plis->dwReturnValue),szEOL);
      wsprintf(szString, "NPP_GetValue(%#08lx, %s, %#08lx)%s%s", dw1,FormatNPPVariable((NPPVariable)dw2),dw3,szEOL,szRet);
      break;
    case action_npp_set_value:
      wsprintf(szRet, "Returning: %s%s", FormatNPAPIError((int)plis->dwReturnValue),szEOL);
      wsprintf(szString, "NPP_SetValue(%#08lx, %s, %#08lx)%s%s", dw1,FormatNPNVariable((NPNVariable)dw2),dw3,szEOL,szRet);
      break;

    default:
      wsprintf(szString, "Unknown action%s",szEOL);
      break;
  }
  strcat(szOutput, szString);
  strcat(szOutput, szTime);
  strcat(szOutput, szEOI);
  iRet = strlen(szString) + strlen(szRet) + strlen(szTime) + strlen(szEOI) + 1;
  return iRet;
}
