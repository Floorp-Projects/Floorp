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
#include "comstrs.h"
#include "scrpthlp.h"
#include "strconv.h"

static char szDefaultString[] = PROFILE_DEFAULT_STRING;

static void getAllArgStrings(LPSTR szFileName, LPSTR szSection, LPSTR sz1, LPSTR sz2, 
                             LPSTR sz3, LPSTR sz4, LPSTR sz5, LPSTR sz6, LPSTR sz7, 
                             int iSize)
{
  XP_GetPrivateProfileString(szSection, KEY_ARG1, szDefaultString, sz1, iSize, szFileName);
  XP_GetPrivateProfileString(szSection, KEY_ARG2, szDefaultString, sz2, iSize, szFileName);
  XP_GetPrivateProfileString(szSection, KEY_ARG3, szDefaultString, sz3, iSize, szFileName);
  XP_GetPrivateProfileString(szSection, KEY_ARG4, szDefaultString, sz4, iSize, szFileName);
  XP_GetPrivateProfileString(szSection, KEY_ARG5, szDefaultString, sz5, iSize, szFileName);
  XP_GetPrivateProfileString(szSection, KEY_ARG6, szDefaultString, sz6, iSize, szFileName);
  XP_GetPrivateProfileString(szSection, KEY_ARG7, szDefaultString, sz7, iSize, szFileName);
}

ScriptItemStruct * readProfileSectionAndCreateScriptItemStruct(LPSTR szFileName, LPSTR szSection)
{
  char szString[256];

  XP_GetPrivateProfileString(szSection, KEY_ACTION, szDefaultString, szString, sizeof(szString), szFileName);

  if(strcmp(szString, szDefaultString) == 0)
    return NULL;

  ScriptItemStruct * psis = NULL;

  const int iSize = 256;

  char sz1[iSize];
  char sz2[iSize];
  char sz3[iSize];
  char sz4[iSize];
  char sz5[iSize];
  char sz6[iSize];
  char sz7[iSize];

  getAllArgStrings(szFileName, szSection, sz1,sz2,sz3,sz4,sz5,sz6,sz7,iSize);

  DWORD dw1 = (DWORD)&sz1[0];
  DWORD dw2 = (DWORD)&sz2[0];
  DWORD dw3 = (DWORD)&sz3[0];
  DWORD dw4 = (DWORD)&sz4[0];
  DWORD dw5 = (DWORD)&sz5[0];
  DWORD dw6 = (DWORD)&sz6[0];
  DWORD dw7 = (DWORD)&sz7[0];

  char szDelay[32];
  XP_GetPrivateProfileString(szSection, KEY_DELAY, "0", szDelay, sizeof(szDelay), szFileName);
  DWORD dwDelay = (DWORD)atol(szDelay);

  if(strcmp(szString, STRING_NPN_VERSION) == 0)
    psis = makeScriptItemStruct(action_npn_version, dw1,dw2,dw3,dw4,dw5,dw6,dw7,dwDelay);
  else if(strcmp(szString, STRING_NPN_GETURL) == 0)
    psis = makeScriptItemStruct(action_npn_get_url, dw1,dw2,dw3,dw4,dw5,dw6,dw7,dwDelay);
  else if(strcmp(szString, STRING_NPN_GETURLNOTIFY) == 0)
    psis = makeScriptItemStruct(action_npn_get_url_notify, dw1,dw2,dw3,dw4,dw5,dw6,dw7,dwDelay);
  else if(strcmp(szString, STRING_NPN_POSTURL) == 0)
    psis = makeScriptItemStruct(action_npn_post_url, dw1,dw2,dw3,dw4,dw5,dw6,dw7,dwDelay);
  else if(strcmp(szString, STRING_NPN_POSTURLNOTIFY) == 0)
    psis = makeScriptItemStruct(action_npn_post_url_notify, dw1,dw2,dw3,dw4,dw5,dw6,dw7,dwDelay);
  else if(strcmp(szString, STRING_NPN_NEWSTREAM) == 0)
    psis = makeScriptItemStruct(action_npn_new_stream, dw1,dw2,dw3,dw4,dw5,dw6,dw7,dwDelay);
  else if(strcmp(szString, STRING_NPN_DESTROYSTREAM) == 0)
    psis = makeScriptItemStruct(action_npn_destroy_stream, dw1,dw2,dw3,dw4,dw5,dw6,dw7,dwDelay);
  else if(strcmp(szString, STRING_NPN_REQUESTREAD) == 0)
    psis = makeScriptItemStruct(action_npn_request_read, dw1,dw2,dw3,dw4,dw5,dw6,dw7,dwDelay);
  else if(strcmp(szString, STRING_NPN_WRITE) == 0)
    psis = makeScriptItemStruct(action_npn_write, dw1,dw2,dw3,dw4,dw5,dw6,dw7,dwDelay);
  else if(strcmp(szString, STRING_NPN_STATUS) == 0)
    psis = makeScriptItemStruct(action_npn_status, dw1,dw2,dw3,dw4,dw5,dw6,dw7,dwDelay);
  else if(strcmp(szString, STRING_NPN_USERAGENT) == 0)
    psis = makeScriptItemStruct(action_npn_user_agent, dw1,dw2,dw3,dw4,dw5,dw6,dw7,dwDelay);
  else if(strcmp(szString, STRING_NPN_MEMALLOC) == 0)
    psis = makeScriptItemStruct(action_npn_mem_alloc, dw1,dw2,dw3,dw4,dw5,dw6,dw7,dwDelay);
  else if(strcmp(szString, STRING_NPN_MEMFREE) == 0)
    psis = makeScriptItemStruct(action_npn_mem_free, dw1,dw2,dw3,dw4,dw5,dw6,dw7,dwDelay);
  else if(strcmp(szString, STRING_NPN_MEMFLUSH) == 0)
    psis = makeScriptItemStruct(action_npn_mem_flush, dw1,dw2,dw3,dw4,dw5,dw6,dw7,dwDelay);
  else if(strcmp(szString, STRING_NPN_RELOADPLUGINS) == 0)
    psis = makeScriptItemStruct(action_npn_reload_plugins, dw1,dw2,dw3,dw4,dw5,dw6,dw7,dwDelay);
  else if(strcmp(szString, STRING_NPN_GETJAVAENV) == 0)
    psis = makeScriptItemStruct(action_npn_get_java_env, dw1,dw2,dw3,dw4,dw5,dw6,dw7,dwDelay);
  else if(strcmp(szString, STRING_NPN_GETJAVAPEER) == 0)
    psis = makeScriptItemStruct(action_npn_get_java_peer, dw1,dw2,dw3,dw4,dw5,dw6,dw7,dwDelay);
  else if(strcmp(szString, STRING_NPN_GETVALUE) == 0)
    psis = makeScriptItemStruct(action_npn_get_value, dw1,dw2,dw3,dw4,dw5,dw6,dw7,dwDelay);
  else if(strcmp(szString, STRING_NPN_SETVALUE) == 0)
  {
    dw4 = (DWORD)XP_GetPrivateProfileInt(szSection, KEY_WIDTH, 0, szFileName);
    dw5 = (DWORD)XP_GetPrivateProfileInt(szSection, KEY_HEIGHT, 0, szFileName);
    psis = makeScriptItemStruct(action_npn_set_value, dw1,dw2,dw3,dw4,dw5,dw6,dw7,dwDelay);
  }
  else if(strcmp(szString, STRING_NPN_INVALIDATERECT) == 0)
  {
    dw3 = (DWORD)XP_GetPrivateProfileInt(szSection, KEY_TOP, 0, szFileName);
    dw4 = (DWORD)XP_GetPrivateProfileInt(szSection, KEY_LEFT, 0, szFileName);
    dw5 = (DWORD)XP_GetPrivateProfileInt(szSection, KEY_BOTTOM, 0, szFileName);
    dw6 = (DWORD)XP_GetPrivateProfileInt(szSection, KEY_RIGHT, 0, szFileName);
    psis = makeScriptItemStruct(action_npn_invalidate_rect, dw1,dw2,dw3,dw4,dw5,dw6,dw7,dwDelay);
  }
  else if(strcmp(szString, STRING_NPN_INVALIDATEREGION) == 0)
    psis = makeScriptItemStruct(action_npn_invalidate_region, dw1,dw2,dw3,dw4,dw5,dw6,dw7,dwDelay);
  else if(strcmp(szString, STRING_NPN_FORCEREDRAW) == 0)
    psis = makeScriptItemStruct(action_npn_force_redraw, dw1,dw2,dw3,dw4,dw5,dw6,dw7,dwDelay);

  return psis;
}
