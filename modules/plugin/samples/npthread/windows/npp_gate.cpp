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

#include "windows.h"

#include "npupp.h"
#include "npapi.h"
#include "plugin.h"
#include "dbg.h"

nsPluginThread * thePluginThread = NULL;

jref NPP_GetJavaClass ()
{
  dbgOut1("wrapper: NPP_GetJavaClass");
  jref rv = (jref)thePluginThread->callNPP(action_npp_get_java_class);
  return NULL;
}

NPError NPP_New(NPMIMEType aType,
                NPP aInstance,
                uint16 aMode,
                int16 aArgc,
                char* aArgn[],
                char* aArgv[],
                NPSavedData* aSaved)
{   
  dbgOut1("wrapper: NPP_New");

  NPError rv = NPERR_NO_ERROR;

  // lamely assuming there is only one embed tag on the page!
  
  // if it is first time in, we don't have it yet
  // initiate a thread with plugin running in it
  thePluginThread = new nsPluginThread((DWORD)aType);

  if (!thePluginThread)
    return NPERR_GENERIC_ERROR;

  rv = (NPError)thePluginThread->callNPP(action_npp_new, 
                                         (DWORD)aType, 
                                         (DWORD)aInstance, 
                                         (DWORD)aMode, 
                                         (DWORD)aArgc, 
                                         (DWORD)aArgn, 
                                         (DWORD)aArgv, 
                                         (DWORD)aSaved);

  return rv;
}

NPError NPP_Destroy (NPP aInstance, NPSavedData** aSave)
{
  dbgOut1("wrapper: NPP_Destroy");

  if (!thePluginThread)
    return NPERR_GENERIC_ERROR;

  NPError ret = (NPError)thePluginThread->callNPP(action_npp_destroy, (DWORD)aInstance, (DWORD)aSave);

  delete thePluginThread;
  thePluginThread = NULL;

  return ret;
}

NPError NPP_SetWindow (NPP aInstance, NPWindow* aNPWindow)
{    
  dbgOut1("wrapper: NPP_SetWindow");

  NPError rv = (NPError)thePluginThread->callNPP(action_npp_set_window, (DWORD)aInstance, (DWORD)aNPWindow);
  return rv;
}

NPError NPP_NewStream(NPP aInstance,
                      NPMIMEType aType,
                      NPStream* aStream, 
                      NPBool aSeekable,
                      uint16* aStype)
{
  dbgOut1("wrapper: NPP_NewStream");

  NPError rv = (NPError)thePluginThread->callNPP(action_npp_new_stream, (DWORD)aInstance, (DWORD)aType, (DWORD)aStream, (DWORD)aSeekable, (DWORD)aStype);
  return rv;
}

int32 NPP_WriteReady (NPP aInstance, NPStream *aStream)
{
  dbgOut1("wrapper: NPP_WriteReady");

  int32 rv = (int32)thePluginThread->callNPP(action_npp_write_ready, (DWORD)aInstance, (DWORD)aStream);
  return rv;
}

int32 NPP_Write (NPP aInstance, NPStream *aStream, int32 aOffset, int32 len, void *aBuffer)
{   
  dbgOut1("wrapper: NPP_Write");

  int32 rv = (int32)thePluginThread->callNPP(action_npp_write, (DWORD)aInstance, (DWORD)aStream, (DWORD)aOffset, (DWORD)len, (DWORD)aBuffer);
  return rv;
}

NPError NPP_DestroyStream (NPP aInstance, NPStream *aStream, NPError aReason)
{
  dbgOut1("wrapper: NPP_DestroyStream");

  NPError rv = (NPError)thePluginThread->callNPP(action_npp_destroy_stream, (DWORD)aInstance, (DWORD)aStream, (DWORD)aReason);
  return rv;
}

void NPP_StreamAsFile (NPP aInstance, NPStream* aStream, const char* aName)
{
  dbgOut1("wrapper: NPP_StreamAsFile");

  thePluginThread->callNPP(action_npp_stream_as_file, (DWORD)aInstance, (DWORD)aStream, (DWORD)aName);
}

void NPP_Print (NPP aInstance, NPPrint* aPrintInfo)
{
  dbgOut1("wrapper: NPP_Print");

  thePluginThread->callNPP(action_npp_print, (DWORD)aInstance, (DWORD)aPrintInfo);
}

void NPP_URLNotify(NPP aInstance, const char* aUrl, NPReason aReason, void* aNotifyData)
{
  dbgOut1("wrapper: NPP_URLNotify");

  thePluginThread->callNPP(action_npp_url_notify, (DWORD)aInstance, (DWORD)aUrl, (DWORD)aReason, (DWORD)aNotifyData);
}

NPError	NPP_GetValue(NPP aInstance, NPPVariable aVariable, void *aValue)
{
  dbgOut1("wrapper: NPP_GetValue");

  NPError rv = (NPError)thePluginThread->callNPP(action_npp_get_value, (DWORD)aInstance, (DWORD)aVariable, (DWORD)aValue);
  return rv;
}

NPError NPP_SetValue(NPP aInstance, NPNVariable aVariable, void *aValue)
{
  dbgOut1("wrapper: NPP_SetValue");

  NPError rv = (NPError)thePluginThread->callNPP(action_npp_set_value, (DWORD)aInstance, (DWORD)aVariable, (DWORD)aValue);
  return rv;
}

int16	NPP_HandleEvent(NPP aInstance, void* aEvent)
{
  dbgOut1("wrapper: NPP_HandleEvent");

  int16 rv = (int16)thePluginThread->callNPP(action_npp_handle_event, (DWORD)aInstance, (DWORD)aEvent);
  return rv;
}
