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

////////////////////////////////////////////////////////////
//
// Implementation of plugin entry points (NPP_*)
//
#include "plugbase.h"
#include "logger.h"

extern CLogger * pLogger;
static char szINIFile[] = NPAPI_INI_FILE_NAME;

// here the plugin creates a plugin instance object which 
// will be associated with this newly created NPP instance and 
// will do all the neccessary job
NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char* argn[], char* argv[], NPSavedData* saved)
{   
  DWORD dwTickEnter = XP_GetTickCount();
  NPError ret = NPERR_NO_ERROR;
  CPluginBase * pPlugin = NULL;

  if(!instance) {
    ret = NPERR_INVALID_INSTANCE_ERROR;
    goto Return;
  }

  pPlugin = (CPluginBase *)CreatePlugin(instance, mode);

  if(!pPlugin) {
    ret = NPERR_OUT_OF_MEMORY_ERROR;
    goto Return;
  }

  instance->pdata = (void *)pPlugin;
  pLogger->associate(pPlugin);

  char szFileName[_MAX_PATH];
  pPlugin->getModulePath(szFileName, sizeof(szFileName));
  strcat(szFileName, szINIFile);
  pLogger->restorePreferences(szFileName);
  
Return:
  DWORD dwTickReturn = XP_GetTickCount();
  pLogger->appendToLog(action_npp_new, dwTickEnter, dwTickReturn, (DWORD)ret, 
                       (DWORD)pluginType, (DWORD)instance, 
                       (DWORD)mode, (DWORD)argc, (DWORD)argn, (DWORD)argv, (DWORD)saved);
  return ret;
}

// here is the place to clean up and destroy the nsPluginInstance object
NPError NPP_Destroy (NPP instance, NPSavedData** save)
{
  DWORD dwTickEnter = XP_GetTickCount();
  NPError ret = NPERR_NO_ERROR;
  CPluginBase * pPlugin = NULL;

  if(!instance) {
    ret = NPERR_INVALID_INSTANCE_ERROR;
    goto Return;
  }

  pPlugin = (CPluginBase *)instance->pdata;
  if(pPlugin) {
    pPlugin->shut();
    DestroyPlugin(pPlugin);
    goto Return;
  }

Return:
  pLogger->blockDumpToFrame(TRUE);
  DWORD dwTickReturn = XP_GetTickCount();
  pLogger->appendToLog(action_npp_destroy, dwTickEnter, dwTickReturn, (DWORD)ret, (DWORD)instance, (DWORD)save);
  pLogger->blockDumpToFrame(FALSE);
  return ret;
}

// during this call we know when the plugin window is ready or
// is about to be destroyed so we can do some gui specific
// initialization and shutdown
NPError NPP_SetWindow (NPP instance, NPWindow* pNPWindow)
{    
  DWORD dwTickEnter = XP_GetTickCount();
  CPluginBase * pPlugin = NULL;
  NPError ret = NPERR_NO_ERROR;

  if(!instance ) {
    ret = NPERR_INVALID_INSTANCE_ERROR;
    goto Return;
  }

  if(!pNPWindow) {
    ret = NPERR_INVALID_INSTANCE_ERROR;
    goto Return;
  }

  pPlugin = (CPluginBase *)instance->pdata;

  if(!pPlugin) {
    ret = NPERR_GENERIC_ERROR;
    goto Return;
  }

  // window just created
  if(!pPlugin->isInitialized() && pNPWindow->window) { 
    if(!pPlugin->init((DWORD)pNPWindow->window)) {
      delete pPlugin;
      pPlugin = NULL;
      ret = NPERR_MODULE_LOAD_FAILED_ERROR;
      goto Return;
    }

    if(pLogger->getShowImmediatelyFlag()) {
      pLogger->dumpLogToTarget();
      pLogger->clearLog();
    }
    goto Return;
  }

  // window goes away
  if(!pNPWindow->window && pPlugin->isInitialized()) {
    pPlugin->shut();
    ret = NPERR_NO_ERROR;
    goto Return;
  }

  // window resized?
  if(pPlugin->isInitialized() && pNPWindow->window) {
    ret = NPERR_NO_ERROR;
    goto Return;
  }

  // this should not happen, nothing to do
  if(!pNPWindow->window && !pPlugin->isInitialized()) {
    ret = NPERR_NO_ERROR;
    goto Return;
  }

Return:
  DWORD dwTickReturn = XP_GetTickCount();
  pLogger->appendToLog(action_npp_set_window, dwTickEnter, dwTickReturn, (DWORD)ret, (DWORD)instance, (DWORD)pNPWindow);
  return ret;
}

NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16* stype)
{
  DWORD dwTickEnter = XP_GetTickCount();
  CPluginBase * pPlugin = NULL;
  NPError ret = NPERR_NO_ERROR;

  if(!instance) {
    ret = NPERR_INVALID_INSTANCE_ERROR;
    goto Return;
  }

  pPlugin = (CPluginBase *)instance->pdata;
  pPlugin->onNPP_NewStream(instance, (LPSTR)type, stream, seekable, stype);

Return:
  DWORD dwTickReturn = XP_GetTickCount();
  pLogger->appendToLog(action_npp_new_stream, dwTickEnter, dwTickReturn, (DWORD)ret, (DWORD)instance, 
                       (DWORD)type, (DWORD)stream, (DWORD)seekable, (DWORD)stype);
  return ret;
}

int32 NPP_WriteReady (NPP instance, NPStream *stream)
{
  DWORD dwTickEnter = XP_GetTickCount();
  CPluginBase * pPlugin = NULL;
  int32 ret = 0x0FFFFFFF;

  if(!instance) {
    ret = 0L;
    goto Return;
  }

  pPlugin = (CPluginBase *)instance->pdata;

Return:
  DWORD dwTickReturn = XP_GetTickCount();
  pLogger->appendToLog(action_npp_write_ready, dwTickEnter, dwTickReturn, (DWORD)ret, 
                       (DWORD)instance, (DWORD)stream);
  return ret;
}

int32 NPP_Write (NPP instance, NPStream *stream, int32 offset, int32 len, void *buffer)
{   
  DWORD dwTickEnter = XP_GetTickCount();
  CPluginBase * pPlugin = NULL;
  int32 ret = len;

  if(!instance)
    goto Return;
  
  pPlugin = (CPluginBase *)instance->pdata;

Return:
  DWORD dwTickReturn = XP_GetTickCount();
  pLogger->appendToLog(action_npp_write, dwTickEnter, dwTickReturn, (DWORD)ret, 
                       (DWORD)instance, (DWORD)stream, 
                       (DWORD)offset, (DWORD)len, (DWORD)buffer);
  return ret;
}

NPError NPP_DestroyStream (NPP instance, NPStream *stream, NPError reason)
{
  DWORD dwTickEnter = XP_GetTickCount();
  CPluginBase * pPlugin = NULL;
  NPError ret = NPERR_NO_ERROR;

  if(!instance) {
    ret = NPERR_INVALID_INSTANCE_ERROR;
    goto Return;
  }

  pPlugin = (CPluginBase *)instance->pdata;

  pPlugin->onNPP_DestroyStream(stream);
  if(pLogger->onNPP_DestroyStream(stream))
    return ret;

Return:
  DWORD dwTickReturn = XP_GetTickCount();
  pLogger->appendToLog(action_npp_destroy_stream, dwTickEnter, dwTickReturn, (DWORD)ret, 
                       (DWORD)instance, (DWORD)stream, (DWORD)reason);
  return ret;
}

void NPP_StreamAsFile (NPP instance, NPStream* stream, const char* fname)
{
  DWORD dwTickEnter = XP_GetTickCount();
  CPluginBase * pPlugin = NULL;

  if(!instance)
    goto Return;

  pPlugin = (CPluginBase *)instance->pdata;

  pPlugin->onNPP_StreamAsFile(instance, stream, fname);

Return:
  DWORD dwTickReturn = XP_GetTickCount();
  pLogger->appendToLog(action_npp_stream_as_file, dwTickEnter, dwTickReturn, 0L, 
                       (DWORD)instance, (DWORD)stream, (DWORD)fname);
}

void NPP_Print (NPP instance, NPPrint* printInfo)
{
  DWORD dwTickEnter = XP_GetTickCount();
  CPluginBase * pPlugin = NULL;

  if(!instance)
    goto Return;

  pPlugin = (CPluginBase *)instance->pdata;

Return:
  DWORD dwTickReturn = XP_GetTickCount();
  pLogger->appendToLog(action_npp_print, dwTickEnter, dwTickReturn, 0L, 
                       (DWORD)instance, (DWORD)printInfo);
}

void NPP_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData)
{
  DWORD dwTickEnter = XP_GetTickCount();
  CPluginBase * pPlugin = NULL;

  if(!instance)
    goto Return;

  pPlugin = (CPluginBase *)instance->pdata;

Return:
  DWORD dwTickReturn = XP_GetTickCount();
  pLogger->appendToLog(action_npp_url_notify, dwTickEnter, dwTickReturn, 0L, 
                       (DWORD)instance, (DWORD)url, (DWORD)reason, (DWORD)notifyData);
}

NPError	NPP_GetValue(NPP instance, NPPVariable variable, void *value)
{
  DWORD dwTickEnter = XP_GetTickCount();
  CPluginBase * pPlugin = NULL;
  NPError ret = NPERR_NO_ERROR;

  if(!instance) {
    ret = NPERR_INVALID_INSTANCE_ERROR;
    goto Return;
  }

  pPlugin = (CPluginBase *)instance->pdata;

#ifdef XP_UNIX
  switch (variable) {
    case NPPVpluginNameString:   
      *((char **)value) = "API Tester plugin";
      break;
    case NPPVpluginDescriptionString:
      *((char **)value) = "This plugins reads and executes test scripts.";
      break;
    default:
      ret = NPERR_GENERIC_ERROR;
  }
#endif // XP_UNIX (displays name and description)

Return:
  DWORD dwTickReturn = XP_GetTickCount();
  pLogger->appendToLog(action_npp_get_value, dwTickEnter, dwTickReturn, (DWORD)ret, 
                       (DWORD)instance, (DWORD)variable, (DWORD)value);
  return ret;
}

NPError NPP_SetValue(NPP instance, NPNVariable variable, void *value)
{
  DWORD dwTickEnter = XP_GetTickCount();
  CPluginBase * pPlugin = NULL;
  NPError ret = NPERR_NO_ERROR;

  if(!instance) {
    ret = NPERR_INVALID_INSTANCE_ERROR;
    goto Return;
  }

  pPlugin = (CPluginBase *)instance->pdata;

Return:
  DWORD dwTickReturn = XP_GetTickCount();
  pLogger->appendToLog(action_npp_set_value, dwTickEnter, dwTickReturn, (DWORD)ret, 
                       (DWORD)instance, (DWORD)variable, (DWORD)value);
  return ret;
}

int16	NPP_HandleEvent(NPP instance, void* event)
{
  DWORD dwTickEnter = XP_GetTickCount();
  CPluginBase * pPlugin = NULL;
  int16 ret = (int16)TRUE;

  if(!instance)
    goto Return;

  pPlugin = (CPluginBase *)instance->pdata;

Return:
  DWORD dwTickReturn = XP_GetTickCount();
  pLogger->appendToLog(action_npp_handle_event, dwTickEnter, dwTickReturn, (DWORD)ret, 
                       (DWORD)instance, (DWORD)event);
  return ret;
}

jref NPP_GetJavaClass (void)
{
  DWORD dwTickEnter = XP_GetTickCount();
  DWORD dwTickReturn = XP_GetTickCount();
  if(pLogger)
    pLogger->appendToLog(action_npp_get_java_class, dwTickEnter, dwTickReturn, 0L);
  return NULL;
}

/**************************************************/
/*                                                */
/*                     Mac                        */
/*                                                */
/**************************************************/

// Mac needs these wrappers, see npplat.h for more info

#ifdef XP_MAC

NPError	Private_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char* argn[], char* argv[], NPSavedData* saved)
{
  EnterCodeResource();
  NPError rv = NPP_New(pluginType, instance, mode, argc, argn, argv, saved);
  ExitCodeResource();
  return rv;	
}

NPError Private_Destroy(NPP instance, NPSavedData** save)
{
  EnterCodeResource();
  NPError rv = NPP_Destroy(instance, save);
  ExitCodeResource();
  return rv;
}

NPError Private_SetWindow(NPP instance, NPWindow* window)
{
  EnterCodeResource();
  NPError rv = NPP_SetWindow(instance, window);
  ExitCodeResource();
  return rv;
}

NPError Private_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16* stype)
{
  EnterCodeResource();
  NPError rv = NPP_NewStream(instance, type, stream, seekable, stype);
  ExitCodeResource();
  return rv;
}

int32 Private_WriteReady(NPP instance, NPStream* stream)
{
  EnterCodeResource();
  int32 rv = NPP_WriteReady(instance, stream);
  ExitCodeResource();
  return rv;
}

int32 Private_Write(NPP instance, NPStream* stream, int32 offset, int32 len, void* buffer)
{
  EnterCodeResource();
  int32 rv = NPP_Write(instance, stream, offset, len, buffer);
  ExitCodeResource();
  return rv;
}

void Private_StreamAsFile(NPP instance, NPStream* stream, const char* fname)
{
  EnterCodeResource();
  NPP_StreamAsFile(instance, stream, fname);
  ExitCodeResource();
}


NPError Private_DestroyStream(NPP instance, NPStream* stream, NPError reason)
{
  EnterCodeResource();
  NPError rv = NPP_DestroyStream(instance, stream, reason);
  ExitCodeResource();
  return rv;
}

int16 Private_HandleEvent(NPP instance, void* event)
{
  EnterCodeResource();
  int16 rv = NPP_HandleEvent(instance, event);
  ExitCodeResource();
  return rv;
}

void Private_Print(NPP instance, NPPrint* platformPrint)
{
  EnterCodeResource();
  NPP_Print(instance, platformPrint);
  ExitCodeResource();
}

void Private_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData)
{
  EnterCodeResource();
  NPP_URLNotify(instance, url, reason, notifyData);
  ExitCodeResource();
}

jref Private_GetJavaClass(void)
{
  return NULL;
}

NPError Private_GetValue(NPP instance, NPPVariable variable, void *result)
{
  EnterCodeResource();
  NPError rv = NPP_GetValue(instance, variable, result);
  ExitCodeResource();
  return rv;
}

NPError Private_SetValue(NPP instance, NPNVariable variable, void *value)
{
  EnterCodeResource();
  NPError rv = NPP_SetValue(instance, variable, value);
  ExitCodeResource();
  return rv;
}

#endif //XP_MAC
