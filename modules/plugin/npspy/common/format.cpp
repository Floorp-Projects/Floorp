/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 *
 */

#include "xp.h"

#include "format.h"
#include "logger.h"

extern Logger * logger;

char * FormatNPAPIError(int iError)
{
  static char szError[64];
  switch (iError)
  {
    case NPERR_NO_ERROR:
      sprintf(szError, "NPERR_NO_ERROR");
      break;
    case NPERR_GENERIC_ERROR:
      sprintf(szError, "NPERR_GENERIC_ERROR");
      break;
    case NPERR_INVALID_INSTANCE_ERROR:
      sprintf(szError, "NPERR_INVALID_INSTANCE_ERROR");
      break;
    case NPERR_INVALID_FUNCTABLE_ERROR:
      sprintf(szError, "NPERR_INVALID_FUNCTABLE_ERROR");
      break;
    case NPERR_MODULE_LOAD_FAILED_ERROR:
      sprintf(szError, "NPERR_MODULE_LOAD_FAILED_ERROR");
      break;
    case NPERR_OUT_OF_MEMORY_ERROR:
      sprintf(szError, "NPERR_OUT_OF_MEMORY_ERROR");
      break;
    case NPERR_INVALID_PLUGIN_ERROR:
      sprintf(szError, "NPERR_INVALID_PLUGIN_ERROR");
      break;
    case NPERR_INVALID_PLUGIN_DIR_ERROR:
      sprintf(szError, "NPERR_INVALID_PLUGIN_DIR_ERROR");
      break;
    case NPERR_INCOMPATIBLE_VERSION_ERROR:
      sprintf(szError, "NPERR_INCOMPATIBLE_VERSION_ERROR");
      break;
    case NPERR_INVALID_PARAM:
      sprintf(szError, "NPERR_INVALID_PARAM");
      break;
    case NPERR_INVALID_URL:
      sprintf(szError, "NPERR_INVALID_URL");
      break;
    case NPERR_FILE_NOT_FOUND:
      sprintf(szError, "NPERR_FILE_NOT_FOUND");
      break;
    case NPERR_NO_DATA:
      sprintf(szError, "NPERR_NO_DATA");
      break;
    case NPERR_STREAM_NOT_SEEKABLE:
      sprintf(szError, "NPERR_STREAM_NOT_SEEKABLE");
      break;
    default:
      sprintf(szError, "Unlisted error");
      break;
  }
  return &szError[0];
}

char * FormatNPAPIReason(int iReason)
{
  static char szReason[64];
  switch (iReason)
  {
    case NPRES_DONE:
      sprintf(szReason, "NPRES_DONE");
      break;
    case NPRES_NETWORK_ERR:
      sprintf(szReason, "NPRES_NETWORK_ERR");
      break;
    case NPRES_USER_BREAK:
      sprintf(szReason, "NPRES_USER_BREAK");
      break;
    default:
      sprintf(szReason, "Unlisted reason");
      break;
  }
  return &szReason[0];
}

char * FormatNPNVariable(NPNVariable var)
{
  static char szVar[80];
  switch (var)
  {
    case NPNVxDisplay:
      sprintf(szVar, "%i -- NPNVxDisplay", var);
      break;
    case NPNVxtAppContext:
      sprintf(szVar, "%i -- NPNVxtAppContext", var);
      break;
    case NPNVnetscapeWindow:
      sprintf(szVar, "%i -- NPNVnetscapeWindow", var);
      break;
    case NPNVjavascriptEnabledBool:
      sprintf(szVar, "%i -- NPNVjavascriptEnabledBool", var);
      break;
    case NPNVasdEnabledBool:
      sprintf(szVar, "%i -- NPNVasdEnabledBool", var);
      break;
    case NPNVisOfflineBool:
      sprintf(szVar, "%i -- NPNVisOfflineBool", var);
      break;
    default:
      sprintf(szVar, "%i -- Unlisted variable", var);
      break;
  }
  return &szVar[0];
}

char * FormatNPPVariable(NPPVariable var)
{
  static char szVar[80];
  switch (var)
  {
    case NPPVpluginNameString:
      sprintf(szVar, "%i -- NPPVpluginNameString", var);
      break;
    case NPPVpluginDescriptionString:
      sprintf(szVar, "%i -- NPPVpluginDescriptionString?", var);
      break;
    case NPPVpluginWindowBool:
      sprintf(szVar, "%i -- NPPVpluginWindowBool?", var);
      break;
    case NPPVpluginTransparentBool:
      sprintf(szVar, "%i -- NPPVpluginTransparentBool?", var);
      break;
    case NPPVjavaClass:
      sprintf(szVar, "%i -- NPPVjavaClass?", var);
      break;
    case NPPVpluginWindowSize:
      sprintf(szVar, "%i -- NPPVpluginWindowSize?", var);
      break;
    case NPPVpluginTimerInterval:
      sprintf(szVar, "%i -- NPPVpluginTimerInterval?", var);
      break;
    case NPPVpluginScriptableInstance:
      sprintf(szVar, "%i -- NPPVpluginScriptableInstance?", var);
      break;
    case NPPVpluginScriptableIID:
      sprintf(szVar, "%i -- NPPVpluginScriptableIID?", var);
      break;
    default:
      sprintf(szVar, "%i -- Unlisted variable?", var);
      break;
  }
  return &szVar[0];
}

BOOL FormatPCHARArgument(char * szBuf, int iLength, LogArgumentStruct * parg)
{
  if(iLength <= parg->iLength)
    return FALSE;

  if(parg->pData == NULL)
    sprintf(szBuf, "%#08lx", parg->dwArg);
  else
    sprintf(szBuf, "%#08lx(\"%s\")", parg->dwArg, (char *)parg->pData);
  return TRUE;
}

BOOL FormatBOOLArgument(char * szBuf, int iLength, LogArgumentStruct * parg)
{
  if(iLength <= 8)
    return FALSE;

  sprintf(szBuf, "%s", ((NPBool)parg->dwArg == TRUE) ? "TRUE" : "FALSE");
  return TRUE;
}

BOOL FormatPBOOLArgument(char * szBuf, int iLength, LogArgumentStruct * parg)
{
  if(iLength <= 8)
    return FALSE;

  sprintf(szBuf, "%#08lx(%s)", parg->dwArg, (*((NPBool *)parg->pData) == TRUE) ? "TRUE" : "FALSE");
  return TRUE;
}

static void makeAbbreviatedString(char * szBuf, int iSize, DWORD dwArg, int iLength, int iWrap)
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
                                  DWORD dw1, DWORD dw2, DWORD dw3, DWORD dw4, 
                                  DWORD dw5, DWORD dw6, DWORD dw7, BOOL bShort)
{
  int iWrap = 10;

  LogItemStruct * plis = new LogItemStruct;
  if(plis == NULL)
    return NULL;

  plis->action = action;
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
        plis->arg2.iLength = strlen((char *)dw2) + 1;
        plis->arg2.pData = new char[plis->arg2.iLength];
        memcpy(plis->arg2.pData, (LPVOID)dw2, plis->arg2.iLength);
      }

      if(dw3 != 0L)
      {
        makeAbbreviatedString(szTarget, sizeof(szTarget), dw3, strlen((char *)dw3), iWrap);
        plis->arg3.iLength = strlen(szTarget) + 1;
        plis->arg3.pData = new char[plis->arg3.iLength];
        memcpy(plis->arg3.pData, (LPVOID)&szTarget[0], plis->arg3.iLength);
      }
      break;
    case action_npn_get_url:
    {
      if(dw2 != 0L)
      {
        plis->arg2.iLength = strlen((char *)dw2) + 1;
        plis->arg2.pData = new char[plis->arg2.iLength];
        memcpy(plis->arg2.pData, (LPVOID)dw2, plis->arg2.iLength);
      }

      if(dw3 != 0L)
      {
        makeAbbreviatedString(szTarget, sizeof(szTarget), dw3, strlen((char *)dw3), iWrap);
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
        plis->arg2.iLength = strlen((char *)dw2) + 1;
        plis->arg2.pData = new char[plis->arg2.iLength];
        memcpy(plis->arg2.pData, (LPVOID)dw2, plis->arg2.iLength);
      }

      if(dw3 != 0L)
      {
        makeAbbreviatedString(szTarget, sizeof(szTarget), dw3, strlen((char *)dw3), iWrap);
        plis->arg3.iLength = strlen(szTarget) + 1;
        plis->arg3.pData = new char[plis->arg3.iLength];
        memcpy(plis->arg3.pData, (LPVOID)&szTarget[0], plis->arg3.iLength);
      }

      makeAbbreviatedString(szBuf, sizeof(szBuf), dw5, strlen((char *)dw5), iWrap);
      plis->arg5.iLength = (int)dw4 + 1;
      plis->arg5.pData = new char[plis->arg5.iLength];
      memcpy(plis->arg5.pData, (LPVOID)&szBuf[0], plis->arg5.iLength);

      break;
    }
    case action_npn_post_url:
    {
      if(dw2 != 0L)
      {
        plis->arg2.iLength = strlen((char *)dw2) + 1;
        plis->arg2.pData = new char[plis->arg2.iLength];
        memcpy(plis->arg2.pData, (LPVOID)dw2, plis->arg2.iLength);
      }

      if(dw3 != 0L)
      {
        makeAbbreviatedString(szTarget, sizeof(szTarget), dw3, strlen((char *)dw3), iWrap);
        plis->arg3.iLength = strlen(szTarget) + 1;
        plis->arg3.pData = new char[plis->arg3.iLength];
        memcpy(plis->arg3.pData, (LPVOID)&szTarget[0], plis->arg3.iLength);
      }

      makeAbbreviatedString(szBuf, sizeof(szBuf), dw5, strlen((char *)dw5), iWrap);
      plis->arg5.iLength = (int)dw4 + 1;
      plis->arg5.pData = new char[plis->arg5.iLength];
      memcpy(plis->arg5.pData, (LPVOID)&szBuf[0], plis->arg5.iLength);

      break;
    }
    case action_npn_new_stream:
    {
      if(dw2 != 0L)
      {
        plis->arg2.iLength = strlen((char *)dw2) + 1;
        plis->arg2.pData = new char[plis->arg2.iLength];
        memcpy(plis->arg2.pData, (LPVOID)dw2, plis->arg2.iLength);
      }

      makeAbbreviatedString(szTarget, sizeof(szTarget), dw3, strlen((char *)dw3), iWrap);
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
      makeAbbreviatedString(szBuf, sizeof(szBuf), dw4, strlen((char *)dw4), iWrap);
      plis->arg4.iLength = strlen(szBuf) + 1;
      plis->arg4.pData = new char[plis->arg4.iLength];
      memcpy(plis->arg4.pData, (LPVOID)&szBuf[0], plis->arg4.iLength);
      break;
    }
    case action_npn_status:
      if(dw2 != 0L)
      {
        plis->arg2.iLength = strlen((char *)dw2) + 1;
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
    case action_npp_new:
      plis->arg1.iLength = strlen((char *)dw1) + 1;
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
      plis->arg2.iLength = strlen((char *)dw2) + 1;
      plis->arg2.pData = new char[plis->arg2.iLength];
      memcpy(plis->arg2.pData, (LPVOID)dw2, plis->arg2.iLength);

      plis->arg5.iLength = sizeof(uint16);
      plis->arg5.pData = new char[plis->arg5.iLength];
      memcpy(plis->arg5.pData, (LPVOID)dw5, plis->arg5.iLength);
      break;
    case action_npp_destroy_stream:
      break;
    case action_npp_stream_as_file:
      plis->arg3.iLength = strlen((char *)dw3) + 1;
      plis->arg3.pData = new char[plis->arg3.iLength];
      memcpy(plis->arg3.pData, (LPVOID)dw3, plis->arg3.iLength);
      break;
    case action_npp_write_ready:
      break;
    case action_npp_write:
    {
      if(dw5 != 0L)
      {
        makeAbbreviatedString(szBuf, sizeof(szBuf), dw5, strlen((char *)dw5), iWrap);
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
      plis->arg2.iLength = strlen((char *)dw2) + 1;
      plis->arg2.pData = new char[plis->arg2.iLength];
      memcpy(plis->arg2.pData, (LPVOID)dw2, plis->arg2.iLength);
      break;
    case action_npp_get_java_class:
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

void freeLogItemStruct(LogItemStruct * lis)
{
  if(lis)
    delete lis;
}

int formatLogItem(LogItemStruct * plis, char * szOutput, BOOL bDOSStyle)
{
  int iRet = 0;
  static char szString[1024];
  static char szEOL[8];
  static char szEOI[256];
  static char szEndOfItem[] = "";

  if(bDOSStyle)
  {
    strcpy(szEOL, "\r\n");
    //strcpy(szEOI, szEndOfItem);
    //strcat(szEOI, "\r\n");
  }
  else
  {
    strcpy(szEOL, "\n");
    //strcpy(szEOI, szEndOfItem);
    //strcat(szEOI, "\n");
  }

  szOutput[0] = '\0';

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
        sprintf(szString, "NPN_Version(%#08lx, %#08lx, %#08lx, %#08lx)%s", dw1,dw2,dw3,dw4,szEOL);
      else
        sprintf(szString, "NPN_Version(%#08lx, %#08lx, %#08lx, %#08lx)%s", dw1,dw2,dw3,dw4,szEOL);
      break;
    case action_npn_get_url_notify:
    {
      FormatPCHARArgument(sz2, sizeof(sz2), &plis->arg2);
      FormatPCHARArgument(sz3, sizeof(sz3), &plis->arg3);
      sprintf(szString, "NPN_GetURLNotify(%#08lx, %s, %s, %#08lx)%s", dw1,sz2,sz3,dw4,szEOL);
      break;
    }
    case action_npn_get_url:
    {
      FormatPCHARArgument(sz2, sizeof(sz2), &plis->arg2);
      FormatPCHARArgument(sz3, sizeof(sz3), &plis->arg3);
      sprintf(szString, "NPN_GetURL(%#08lx, %s, %s)%s", dw1,sz2,sz3,szEOL);
      break;
    }
    case action_npn_post_url_notify:
    {
      FormatPCHARArgument(sz2, sizeof(sz2), &plis->arg2);
      FormatPCHARArgument(sz3, sizeof(sz3), &plis->arg3);
      FormatPCHARArgument(sz5, sizeof(sz5), &plis->arg5);
      FormatBOOLArgument(sz6, sizeof(sz6), &plis->arg6);

      sprintf(szString, "NPN_PostURLNotify(%#08lx, %s, %s, %li, %s, %s, %#08lx)%s", 
               dw1,sz2,sz3,(uint32)dw4,sz5,sz6,dw7,szEOL);
      break;
    }
    case action_npn_post_url:
    {
      FormatPCHARArgument(sz2, sizeof(sz2), &plis->arg2);
      FormatPCHARArgument(sz3, sizeof(sz3), &plis->arg3);
      FormatPCHARArgument(sz5, sizeof(sz5), &plis->arg5);
      FormatBOOLArgument(sz6, sizeof(sz6), &plis->arg6);

      sprintf(szString, "NPN_PostURL(%#08lx, %s, %s, %li, %s, %s)%s", 
               dw1,sz2,sz3,(uint32)dw4,sz5,sz6,szEOL);
      break;
    }
    case action_npn_new_stream:
    {
      FormatPCHARArgument(sz2, sizeof(sz2), &plis->arg2);
      FormatPCHARArgument(sz3, sizeof(sz3), &plis->arg3);
      if(plis->arg4.pData != NULL)
        sprintf(szString, "NPN_NewStream(%#08lx, %s, %s, %#08lx(%#08lx))%s", 
                 dw1, sz2,sz3,dw4,*(DWORD *)plis->arg4.pData,szEOL);
      else
        sprintf(szString, "NPN_NewStream(%#08lx, \"%s\", \"%s\", %#08lx)%s", dw1, sz2,sz3,dw4,szEOL);
      break;
    }
    case action_npn_destroy_stream:
      sprintf(szString, "NPN_DestroyStream(%#08lx, %#08lx, %s)%s", dw1,dw2,FormatNPAPIReason((int)dw3),szEOL);
      break;
    case action_npn_request_read:
      sprintf(szString, "NPN_RequestRead(%#08lx, %#08lx)%s", dw1, dw2, szEOL);
      break;
    case action_npn_write:
    {
      FormatPCHARArgument(sz4, sizeof(sz4), &plis->arg4);
      sprintf(szString, "NPN_Write(%#08lx, %#08lx, %li, %s)%s", dw1, dw2, (int32)dw3, sz4, szEOL);
      break;
    }
    case action_npn_status:
    {
      FormatPCHARArgument(sz2, sizeof(sz2), &plis->arg2);
      sprintf(szString, "NPN_Status(%#08lx, %s)%s", dw1, sz2, szEOL);
      break;
    }
    case action_npn_user_agent:
      sprintf(szString, "NPN_UserAgent(%#08lx)%s", dw1, szEOL);
      break;
    case action_npn_mem_alloc:
      sprintf(szString, "NPN_MemAlloc(%li)%s", dw1, szEOL);
      break;
    case action_npn_mem_free:
      sprintf(szString, "NPN_MemFree(%#08lx)%s", dw1,szEOL);
      break;
    case action_npn_mem_flush:
      sprintf(szString, "NPN_MemFlush(%li)%s", dw1, szEOL);
      break;
    case action_npn_reload_plugins:
    {
      FormatBOOLArgument(sz1, sizeof(sz1), &plis->arg1);
      sprintf(szString, "NPN_ReloadPlugins(%s)%s", sz1,szEOL);
      break;
    }
    case action_npn_get_java_env:
      sprintf(szString, "NPN_GetJavaEnv()%s", szEOL);
      break;
    case action_npn_get_java_peer:
      sprintf(szString, "NPN_GetJavaPeer(%#08lx)%s", dw1, szEOL);
      break;
    case action_npn_get_value:
    {
      switch(dw2)
      {
        case NPNVxDisplay:
        case NPNVxtAppContext:
        case NPNVnetscapeWindow:
          if(dw3 != 0L)
            sprintf(szString, "NPN_GetValue(%#08lx, %s, %#08lx(%#08lx))%s",dw1,FormatNPNVariable((NPNVariable)dw2),dw3,*(DWORD *)dw3,szEOL);
          else
            sprintf(szString, "NPN_GetValue(%#08lx, %s, %#08lx)%s",dw1,FormatNPNVariable((NPNVariable)dw2),dw3,szEOL);
          break;
        case NPNVjavascriptEnabledBool:
        case NPNVasdEnabledBool:
        case NPNVisOfflineBool:
          if(dw3 != 0L)
            sprintf(szString, "NPN_GetValue(%#08lx, %s, %#08lx(%s))%s",
                     dw1,FormatNPNVariable((NPNVariable)dw2),dw3,
                     (((NPBool)*(DWORD *)dw3) == TRUE) ? "TRUE" : "FALSE", szEOL);
          else
            sprintf(szString, "NPN_GetValue(%#08lx, %s, %#08lx)%s",dw1,FormatNPNVariable((NPNVariable)dw2),dw3,szEOL);
          break;
        default:
          break;
      }
      break;
    }
    case action_npn_set_value:

      if(((NPPVariable)dw2 == NPPVpluginNameString) || ((NPPVariable)dw2 == NPPVpluginDescriptionString))
      {
        FormatPCHARArgument(sz3, sizeof(sz3), &plis->arg3);
        sprintf(szString, "NPN_SetValue(%#08lx, %s, %s)%s", dw1,FormatNPPVariable((NPPVariable)dw2),sz3,szEOL);
      }
      else if(((NPPVariable)dw2 == NPPVpluginWindowBool) || ((NPPVariable)dw2 == NPPVpluginTransparentBool))
      {
        FormatPBOOLArgument(sz3, sizeof(sz3), &plis->arg3);
        sprintf(szString, "NPN_SetValue(%#08lx, %s, %s)%s", 
                 dw1,FormatNPPVariable((NPPVariable)dw2),sz3,szEOL);
      }
      else if((NPPVariable)dw2 == NPPVpluginWindowSize)
      {
        if(plis->arg3.pData != NULL)
        {
          int32 iWidth = ((NPSize *)plis->arg3.pData)->width;
          int32 iHeight = ((NPSize *)plis->arg3.pData)->height;
          sprintf(szString, "NPN_SetValue(%#08lx, %s, %#08lx(%li,%li))%s", 
                   dw1,FormatNPPVariable((NPPVariable)dw2),dw3,iWidth,iHeight,szEOL);
        }
        else
          sprintf(szString, "NPN_SetValue(%#08lx, %s, %#08lx(?,?))%s", 
                   dw1,FormatNPPVariable((NPPVariable)dw2),dw3,szEOL);
      }
      else
        sprintf(szString, "NPN_SetValue(%#08lx, %s, %#08lx(What is it?))%s", dw1,FormatNPPVariable((NPPVariable)dw2),dw3,szEOL);
      break;
    case action_npn_invalidate_rect:
    {
      if(plis->arg2.pData != NULL)
      {
        uint16 top    = ((NPRect *)plis->arg2.pData)->top;
        uint16 left   = ((NPRect *)plis->arg2.pData)->left;
        uint16 bottom = ((NPRect *)plis->arg2.pData)->bottom;
        uint16 right  = ((NPRect *)plis->arg2.pData)->right;
        sprintf(szString, "NPN_InvalidateRect(%#08lx, %#08lx(%u,%u;%u,%u)%s", dw1,dw2,top,left,bottom,right,szEOL);
      }
      else
        sprintf(szString, "NPN_InvalidateRect(%#08lx, %#08lx(?,?,?,?)%s", dw1,dw2,szEOL);
      break;
    }
    case action_npn_invalidate_region:
      sprintf(szString, "NPN_InvalidateRegion(%#08lx, %#08lx)%s", dw1,dw2,szEOL);
      break;
    case action_npn_force_redraw:
      sprintf(szString, "NPN_ForceRedraw(%#08lx)%s", dw1,szEOL);
      break;

    // NPP action
    case action_npp_new:
    {
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
      sprintf(szString, "NPP_New(\"%s\", %#08lx, %s, %i, %#08lx, %#08lx, %#08lx)%s", 
               (char *)dw1,dw2,szMode,(int)dw4,dw5,dw6,dw7,szEOL);
      break;
    }
    case action_npp_destroy:
      sprintf(szString, "NPP_Destroy(%#08lx, %#08lx(%#08lx))%s", dw1, dw2, *(DWORD *)plis->arg2.pData,szEOL);
      break;
    case action_npp_set_window:
    {
      char szWindow[512];

      if(plis->arg2.pData != NULL)
      {
        char szType[80];
        switch (((NPWindow*)plis->arg2.pData)->type)
        {
          case NPWindowTypeWindow:
            sprintf(szType, "NPWindowTypeWindow");
            break;
          case NPWindowTypeDrawable:
            sprintf(szType, "NPWindowTypeDrawable");
            break;
          default:
            sprintf(szType, "[Unlisted type]");
            break;
        }
        sprintf(szWindow, "NPWindow: %#08lx, (%li,%li), (%li,%li), (%i,%i,%i,%i), %s", 
                 ((NPWindow*)plis->arg2.pData)->window, 
                 ((NPWindow*)plis->arg2.pData)->x, 
                 ((NPWindow*)plis->arg2.pData)->y, 
                 ((NPWindow*)plis->arg2.pData)->width, 
                 ((NPWindow*)plis->arg2.pData)->height, 
                 ((NPWindow*)plis->arg2.pData)->clipRect.top, 
                 ((NPWindow*)plis->arg2.pData)->clipRect.left, 
                 ((NPWindow*)plis->arg2.pData)->clipRect.bottom, 
                 ((NPWindow*)plis->arg2.pData)->clipRect.right, szType);
        sprintf(szString, "NPP_SetWindow(%#08lx, %#08lx)%s%s%s", dw1,dw2," ",szWindow,szEOL);
      }
      else
        sprintf(szString, "NPP_SetWindow(%#08lx, %#08lx)%s", dw1,dw2,szEOL);

      break;
    }
    case action_npp_new_stream:
    {
      switch (*(int16 *)plis->arg5.pData)
      {
        case NP_NORMAL:
          sprintf(sz5, "NP_NORMAL");
          break;
        case NP_ASFILEONLY:
          sprintf(sz5, "NP_ASFILEONLY");
          break;
        case NP_ASFILE:
          sprintf(sz5, "NP_ASFILE");
          break;
        default:
          sprintf(sz5, "[Unlisted type]");
          break;
      }
      FormatPCHARArgument(sz2, sizeof(sz2), &plis->arg2);
      sprintf(szString, "NPP_NewStream(%#08lx, %s, %#08lx, %s, %s)%s", dw1, sz2, dw3, 
               ((NPBool)dw4 == TRUE) ? "TRUE" : "FALSE", sz5, szEOL);
      break;
    }
    case action_npp_destroy_stream:
      sprintf(szString, "NPP_DestroyStream(%#08lx, %#08lx, %s)%s", dw1,dw2,FormatNPAPIReason((int)dw3),szEOL);
      break;
    case action_npp_stream_as_file:
      FormatPCHARArgument(sz3, sizeof(sz3), &plis->arg3);
      sprintf(szString, "NPP_StreamAsFile(%#08lx, %#08lx, %s)%s", dw1,dw2,sz3,szEOL);
      break;
    case action_npp_write_ready:
      sprintf(szString, "NPP_WriteReady(%#08lx, %#08lx)%s", dw1,dw2,szEOL);
      break;
    case action_npp_write:
    {
      FormatPCHARArgument(sz5, sizeof(sz5), &plis->arg5);
      sprintf(szString, "NPP_Write(%#08lx, %#08lx, %li, %li, %s))%s",dw1,dw2,dw3,dw4,sz5,szEOL);
      break;
    }
    case action_npp_print:
      sprintf(szString, "NPP_Print(%#08lx, %#08lx)%s", dw1, dw2,szEOL);
      break;
    case action_npp_handle_event:
      sprintf(szString, "NPP_HandleEvent(%#08lx, %#08lx)%s", dw1,dw2,szEOL);
      break;
    case action_npp_url_notify:
    {
      FormatPCHARArgument(sz2, sizeof(sz2), &plis->arg2);
      sprintf(szString, "NPP_URLNotify(%#08lx, %s, %s, %#08lx)%s", dw1,sz2,FormatNPAPIReason((int)dw3),dw4,szEOL);
      break;
    }
    case action_npp_get_java_class:
      sprintf(szString, "NPP_GetJavaClass()%s",szEOL);
      break;
    case action_npp_get_value:
      sprintf(szString, "NPP_GetValue(%#08lx, %s, %#08lx)%s", dw1,FormatNPPVariable((NPPVariable)dw2),dw3,szEOL);
      break;
    case action_npp_set_value:
      sprintf(szString, "NPP_SetValue(%#08lx, %s, %#08lx)%s", dw1,FormatNPNVariable((NPNVariable)dw2),dw3,szEOL);
      break;

    default:
      sprintf(szString, "Unknown action%s",szEOL);
      break;
  }
  strcat(szOutput, szString);
  strcat(szOutput, szEOI);
  iRet = strlen(szString) + strlen(szEOI) + 1;
  return iRet;
}
