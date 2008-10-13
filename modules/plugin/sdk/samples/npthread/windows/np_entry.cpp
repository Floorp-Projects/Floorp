/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include "npapi.h"
#include "npfunctions.h"
#include "dbg.h"

NPNetscapeFuncs NPNFuncs;

NPError WINAPI NP_GetEntryPoints(NPPluginFuncs* aPluginFuncs)
{
  dbgOut1("wrapper: NP_GetEntryPoints");

  if(aPluginFuncs == NULL)
    return NPERR_INVALID_FUNCTABLE_ERROR;

  if(aPluginFuncs->size < sizeof(NPPluginFuncs))
    return NPERR_INVALID_FUNCTABLE_ERROR;

  aPluginFuncs->version       = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
  aPluginFuncs->newp          = NPP_New;
  aPluginFuncs->destroy       = NPP_Destroy;
  aPluginFuncs->setwindow     = NPP_SetWindow;
  aPluginFuncs->newstream     = NPP_NewStream;
  aPluginFuncs->destroystream = NPP_DestroyStream;
  aPluginFuncs->asfile        = NPP_StreamAsFile;
  aPluginFuncs->writeready    = NPP_WriteReady;
  aPluginFuncs->write         = NPP_Write;
  aPluginFuncs->print         = NPP_Print;
  aPluginFuncs->event         = NPP_HandleEvent;
  aPluginFuncs->urlnotify     = NPP_URLNotify;
  aPluginFuncs->getvalue      = NPP_GetValue;
  aPluginFuncs->setvalue      = NPP_SetValue;
  aPluginFuncs->javaClass     = NULL;

  return NPERR_NO_ERROR;
}

NPError WINAPI NP_Initialize(NPNetscapeFuncs* aNetscapeFuncs)
{
  dbgOut1("wrapper: NP_Initialize");

  if(aNetscapeFuncs == NULL)
    return NPERR_INVALID_FUNCTABLE_ERROR;

  if(HIBYTE(aNetscapeFuncs->version) > NP_VERSION_MAJOR)
    return NPERR_INCOMPATIBLE_VERSION_ERROR;

  if(aNetscapeFuncs->size < sizeof NPNetscapeFuncs)
    return NPERR_INVALID_FUNCTABLE_ERROR;

  NPNFuncs.size             = aNetscapeFuncs->size;
  NPNFuncs.version          = aNetscapeFuncs->version;
  NPNFuncs.geturlnotify     = aNetscapeFuncs->geturlnotify;
  NPNFuncs.geturl           = aNetscapeFuncs->geturl;
  NPNFuncs.posturlnotify    = aNetscapeFuncs->posturlnotify;
  NPNFuncs.posturl          = aNetscapeFuncs->posturl;
  NPNFuncs.requestread      = aNetscapeFuncs->requestread;
  NPNFuncs.newstream        = aNetscapeFuncs->newstream;
  NPNFuncs.write            = aNetscapeFuncs->write;
  NPNFuncs.destroystream    = aNetscapeFuncs->destroystream;
  NPNFuncs.status           = aNetscapeFuncs->status;
  NPNFuncs.uagent           = aNetscapeFuncs->uagent;
  NPNFuncs.memalloc         = aNetscapeFuncs->memalloc;
  NPNFuncs.memfree          = aNetscapeFuncs->memfree;
  NPNFuncs.memflush         = aNetscapeFuncs->memflush;
  NPNFuncs.reloadplugins    = aNetscapeFuncs->reloadplugins;
  NPNFuncs.getJavaEnv       = NULL;
  NPNFuncs.getJavaPeer      = NULL;
  NPNFuncs.getvalue         = aNetscapeFuncs->getvalue;
  NPNFuncs.setvalue         = aNetscapeFuncs->setvalue;
  NPNFuncs.invalidaterect   = aNetscapeFuncs->invalidaterect;
  NPNFuncs.invalidateregion = aNetscapeFuncs->invalidateregion;
  NPNFuncs.forceredraw      = aNetscapeFuncs->forceredraw;

  return NPERR_NO_ERROR;
}

NPError WINAPI NP_Shutdown()
{
  dbgOut1("wrapper:NP_Shutdown");

  return NPERR_NO_ERROR;
}
