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

#ifndef __ACTIONNAMES_H__
#define __ACTIONNAMES_H__

// the order is important
char * ActionName[] =
{
  "INVALID_CALL",
  "NPN_Version",
  "NPN_GetURLNotify",
  "NPN_GetURL",
  "NPN_PostURLNotify",
  "NPN_PostURL",
  "NPN_RequestRead",
  "NPN_NewStream",
  "NPN_Write",
  "NPN_DestroyStream",
  "NPN_Status",
  "NPN_UserAgent",
  "NPN_MemAlloc",
  "NPN_MemFree",
  "NPN_MemFlush",
  "NPN_ReloadPlugins",
  "NPN_GetJavaEnv",
  "NPN_GetJavaPeer",
  "NPN_GetValue",
  "NPN_SetValue",
  "NPN_InvalidateRect",
  "NPN_InvalidateRegion",
  "NPN_ForceRedraw",
  "NPP_New",
  "NPP_Destroy",
  "NPP_SetWindow",
  "NPP_NewStream",
  "NPP_DestroyStream",
  "NPP_StreamAsFile",
  "NPP_WriteReady",
  "NPP_Write",
  "NPP_Print",
  "NPP_HandleEvent",
  "NPP_URLNotify",
  "NPP_GetJavaClass",
  "NPP_GetValue",
  "NPP_SetValue"
};

#endif
