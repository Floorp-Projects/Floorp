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

#include "scrpthlp.h"
#include "profile.h"
#include "strconv.h"

static char szDefaultString[] = PROFILE_DEFAULT_STRING;

static void makeDataEntry(ScriptArgumentStruct * psas, DWORD dwArgument, int iSize)
{
  if((dwArgument != 0L) && (dwArgument != DEFAULT_DWARG_VALUE) && (iSize > 0))
  {
    psas->iLength = iSize;
    char * p = new char[psas->iLength];
    memcpy(p, (LPVOID)dwArgument, psas->iLength);
    psas->dwArg = (DWORD)p;
  }
  else
  {
    psas->iLength = 0;
    psas->dwArg = dwArgument;
  }
}

ScriptItemStruct * makeScriptItemStruct(NPAPI_Action action, DWORD dw1, DWORD dw2, 
                                        DWORD dw3, DWORD dw4, DWORD dw5, DWORD dw6, 
                                        DWORD dw7, DWORD dwDelay)
{
  ScriptItemStruct * psis = new ScriptItemStruct;
  if(psis == NULL)
    return NULL;

  psis->dwDelay = (dwDelay >= 0L) ? dwDelay : 0L;
  psis->action = action;
  psis->arg1.dwArg = dw1;
  psis->arg2.dwArg = dw2;
  psis->arg3.dwArg = dw3;
  psis->arg4.dwArg = dw4;
  psis->arg5.dwArg = dw5;
  psis->arg6.dwArg = dw6;
  psis->arg7.dwArg = dw7;

  DWORD dwTNVFlags = 0L;

  switch (action)
  {
    case action_invalid:
      break;
    case action_npn_version:
      convertStringToDWORD4(&psis->arg1.dwArg, &psis->arg2.dwArg, &psis->arg3.dwArg, &psis->arg4.dwArg);
      break;
    case action_npn_get_url:
      convertStringToDWORD1(&psis->arg1.dwArg);
      dwTNVFlags = convertStringToLPSTR2(&dw2, &dw3);
      makeDataEntry(&psis->arg2, dw2, (dwTNVFlags & fTNV1) ? 0 : strlen((LPSTR)dw2) + 1);
      makeDataEntry(&psis->arg3, dw3, (dwTNVFlags & fTNV2) ? 0 : strlen((LPSTR)dw3) + 1);
      break;
    case action_npn_get_url_notify:
      convertStringToDWORD2(&psis->arg1.dwArg, &psis->arg4.dwArg);
      dwTNVFlags = convertStringToLPSTR2(&dw2, &dw3);
      makeDataEntry(&psis->arg2, dw2, (dwTNVFlags & fTNV1) ? 0 : strlen((LPSTR)dw2) + 1);
      makeDataEntry(&psis->arg3, dw3, (dwTNVFlags & fTNV2) ? 0 : strlen((LPSTR)dw3) + 1);
      break;
    case action_npn_post_url:
    {
      convertStringToDWORD2(&psis->arg1.dwArg, &psis->arg4.dwArg);
      dwTNVFlags = convertStringToLPSTR3(&dw2, &dw3, &dw5);
      convertStringToBOOL1(&psis->arg6.dwArg);
      makeDataEntry(&psis->arg2, dw2, (dwTNVFlags & fTNV1) ? 0 : strlen((LPSTR)dw2) + 1);
      makeDataEntry(&psis->arg3, dw3, (dwTNVFlags & fTNV2) ? 0 : strlen((LPSTR)dw3) + 1);

      int iSize;
      if(dwTNVFlags & fTNV3)
        iSize = 0;
      else
        if(psis->arg5.dwArg == DEFAULT_DWARG_VALUE)
        {
          iSize = strlen((LPSTR)dw5) + 1;
          psis->arg5.dwArg = iSize - 1;
        }
        else
          iSize = psis->arg5.dwArg;

      makeDataEntry(&psis->arg5, dw5, (dwTNVFlags & fTNV3) ? 0 : strlen((LPSTR)dw5) + 1);
      break;
    }
    case action_npn_post_url_notify:
    {
      convertStringToDWORD3(&psis->arg1.dwArg, &psis->arg4.dwArg, &psis->arg7.dwArg);
      dwTNVFlags = convertStringToLPSTR3(&dw2, &dw3, &dw5);
      convertStringToBOOL1(&psis->arg6.dwArg);
      makeDataEntry(&psis->arg2, dw2, (dwTNVFlags & fTNV1) ? 0 : strlen((LPSTR)dw2) + 1);
      makeDataEntry(&psis->arg3, dw3, (dwTNVFlags & fTNV2) ? 0 : strlen((LPSTR)dw3) + 1);

      int iSize;
      if(dwTNVFlags & fTNV3)
        iSize = 0;
      else
        if(psis->arg5.dwArg == DEFAULT_DWARG_VALUE)
        {
          iSize = strlen((LPSTR)dw5) + 1;
          psis->arg5.dwArg = iSize - 1;
        }
        else
          iSize = psis->arg5.dwArg;

      makeDataEntry(&psis->arg5, dw5, (dwTNVFlags & fTNV3) ? 0 : strlen((LPSTR)dw5) + 1);
      break;
    }
    case action_npn_new_stream:
      convertStringToDWORD2(&psis->arg1.dwArg, &psis->arg4.dwArg);
      dwTNVFlags = convertStringToLPSTR2(&dw2, &dw3);
      makeDataEntry(&psis->arg2, dw2, (dwTNVFlags & fTNV1) ? 0 : strlen((LPSTR)dw2) + 1);
      makeDataEntry(&psis->arg3, dw3, (dwTNVFlags & fTNV2) ? 0 : strlen((LPSTR)dw3) + 1);
      break;
    case action_npn_destroy_stream:
      convertStringToDWORD2(&psis->arg1.dwArg, &psis->arg2.dwArg);
      convertStringToNPReason1(&psis->arg3.dwArg);
      break;
    case action_npn_request_read:
      convertStringToDWORD2(&psis->arg1.dwArg, &psis->arg2.dwArg);
      break;
    case action_npn_write:
    {
      convertStringToDWORD3(&psis->arg1.dwArg, &psis->arg2.dwArg, &psis->arg3.dwArg);
      dwTNVFlags = convertStringToLPSTR1(&dw4);

      int iSize;
      if(dwTNVFlags & fTNV1)
        iSize = 0;
      else
        if(psis->arg3.dwArg == DEFAULT_DWARG_VALUE)
        {
          iSize = strlen((LPSTR)dw4) + 1;
          psis->arg3.dwArg = iSize - 1;
        }
        else
          iSize = psis->arg3.dwArg;

      makeDataEntry(&psis->arg4, dw4, iSize);
      break;
    }
    case action_npn_status:
      convertStringToDWORD1(&psis->arg1.dwArg);
      dwTNVFlags = convertStringToLPSTR1(&dw2);
      makeDataEntry(&psis->arg2, dw2, (dwTNVFlags & fTNV1) ? 0 : strlen((LPSTR)dw2) + 1);
      break;

    case action_npn_user_agent:
    case action_npn_mem_alloc:
    case action_npn_mem_free:
    case action_npn_mem_flush:
    case action_npn_get_java_peer:
      convertStringToDWORD1(&psis->arg1.dwArg);
      break;

    case action_npn_reload_plugins:
      convertStringToBOOL1(&psis->arg1.dwArg);
      break;
    case action_npn_get_java_env:
      break;
    case action_npn_get_value:
      convertStringToDWORD2(&psis->arg1.dwArg, &psis->arg3.dwArg);
      convertStringToNPNVariable1(&psis->arg2.dwArg);
      break;
    case action_npn_set_value:
    {
      convertStringToDWORD1(&psis->arg1.dwArg);
      convertStringToNPPVariable1(&psis->arg2.dwArg);
      NPPVariable nppVar = (NPPVariable)psis->arg2.dwArg;

      switch (nppVar)
      {
        case NPPVpluginNameString:
        case NPPVpluginDescriptionString:
          dwTNVFlags = convertStringToLPSTR1(&dw3);
          makeDataEntry(&psis->arg3, dw3, (dwTNVFlags & fTNV1) ? 0 : strlen((LPSTR)dw3) + 1);
          break;
        case NPPVpluginWindowBool:
        case NPPVpluginTransparentBool:
          dwTNVFlags = convertStringToBOOL1(&dw3);
          makeDataEntry(&psis->arg3, dw3, (dwTNVFlags & fTNV1) ? 0 : sizeof(NPBool));
          break;
        case NPPVpluginWindowSize:
        {
          int iDataSize = 0;
          static NPSize npsize;
 
          convertStringToDWORD1(&psis->arg3.dwArg);

          if(psis->arg3.dwArg == DEFAULT_DWARG_VALUE)
          {
            npsize.width  = dw4;
            npsize.height = dw5;
            dw3 = (DWORD)&npsize;
            iDataSize = sizeof(NPSize);
          }
          makeDataEntry(&psis->arg3, dw3, iDataSize);
          break;
        }
        default:
          break;
      }
      break;
    }
    case action_npn_invalidate_rect:
    {
      int iDataSize = 0;
      static NPRect nprect;

      convertStringToDWORD2(&psis->arg1.dwArg, &psis->arg2.dwArg);

      if(psis->arg2.dwArg == DEFAULT_DWARG_VALUE)
      {
        nprect.top    = (int)dw3;
        nprect.left   = (int)dw4;
        nprect.bottom = (int)dw5;
        nprect.right  = (int)dw6;
        dw2 = (DWORD)&nprect;
        iDataSize = sizeof(NPSize);
      }
      makeDataEntry(&psis->arg2, dw2, iDataSize);
      break;
    }
    case action_npn_invalidate_region:
      convertStringToDWORD2(&psis->arg1.dwArg, &psis->arg2.dwArg);
      break;
    case action_npn_force_redraw:
      convertStringToDWORD1(&psis->arg1.dwArg);
      break;

    default:
      break;
  }

  return psis;
}
