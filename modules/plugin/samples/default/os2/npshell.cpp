/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include <os2.h>
#include <string.h>

#include "npnulos2.h"

#include "Plugin.h" // this includes npapi.h
#include "utils.h"
#include "dbg.h"

char szAppName[] = "NPNULL";

//---------------------------------------------------------------------------
// NPP_Initialize:
//---------------------------------------------------------------------------
NPError NPP_Initialize(void)
{
  RegisterNullPluginWindowClass();
  return NPERR_NO_ERROR;
}

//---------------------------------------------------------------------------
// NPP_Shutdown:
//---------------------------------------------------------------------------
void NPP_Shutdown(void)
{
  UnregisterNullPluginWindowClass();
}

//---------------------------------------------------------------------------
// NPP_New:
//---------------------------------------------------------------------------
NPError NP_LOADDS NPP_New(NPMIMEType pluginType,
                          NPP pInstance,
                          uint16 mode,
                          int16 argc,
                          char* argn[],
                          char* argv[],
                          NPSavedData* saved)
{
  if(pInstance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  // See if the content provider specified from where to fetch the plugin
  char * szPageURL = NULL;
  char * szFileURL = NULL;
  char * szFileExtension = NULL;
  char * buf = NULL;
  BOOL bHidden = FALSE;

  for(int i = 0; i < argc; i++)
  {
    if(strcmpi(argn[i],"pluginspage") == 0 && argv[i] != NULL)
      szPageURL = (char *)argv[i];
    else if(strcmpi(argn[i],"codebase") == 0 && argv[i] != NULL)
      szPageURL = (char *)argv[i];
    else if(strcmpi(argn[i],"pluginurl") == 0 && argv[i] != NULL)
      szFileURL = (char *)argv[i];
    else if(strcmpi(argn[i],"classid") == 0 && argv[i] != NULL)
      szFileURL = (char *)argv[i];
    else if(strcmpi(argn[i],"SRC") == 0 && argv[i] != NULL)
      buf = (char *)argv[i];
    else if(strcmpi(argn[i],"HIDDEN") == 0 && argv[i] != NULL)
      bHidden = (strcmp((char *)argv[i], "TRUE") == 0);
  }

  /* some post-processing on the filename to attempt to extract the extension:  */
  if(buf != NULL)
  {
    buf = strrchr(buf, '.');
    szFileExtension = ++buf;
  }

  CPlugin * pPlugin = new CPlugin(hInst, 
                                  pInstance, 
                                  mode, 
                                  pluginType, 
                                  szPageURL, 
                                  szFileURL, 
                                  szFileExtension,
                                  bHidden);
  if(pPlugin == NULL)
    return NPERR_OUT_OF_MEMORY_ERROR;

  if(bHidden)
  {
    if(!pPlugin->init(NULL))
    {
      delete pPlugin;
      pPlugin = NULL;
      return NPERR_MODULE_LOAD_FAILED_ERROR;
    }
  }

  pInstance->pdata = (void *)pPlugin;

  return NPERR_NO_ERROR;
}

//---------------------------------------------------------------------------
// NPP_Destroy:
//---------------------------------------------------------------------------
NPError NP_LOADDS
NPP_Destroy(NPP pInstance, NPSavedData** save)
{
  dbgOut1("NPP_Destroy");
  if(pInstance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  CPlugin * pPlugin = (CPlugin *)pInstance->pdata;
  if(pPlugin != NULL)
  {
    pPlugin->shut();
    delete pPlugin;
  }

  return NPERR_NO_ERROR;
}

//---------------------------------------------------------------------------
// NPP_SetWindow:
//---------------------------------------------------------------------------
NPError NP_LOADDS NPP_SetWindow(NPP pInstance, NPWindow * pNPWindow)
{
  if(pInstance == NULL)
  {
    dbgOut1("NPP_SetWindow returns NPERR_INVALID_INSTANCE_ERROR");
    return NPERR_INVALID_INSTANCE_ERROR;
  }

  if(pNPWindow == NULL)
  {
    dbgOut1("NPP_SetWindow returns NPERR_GENERIC_ERROR");
    return NPERR_GENERIC_ERROR;
  }

  HWND hWnd = (HWND)pNPWindow->window;

  CPlugin * pPlugin = (CPlugin *)pInstance->pdata;
  assert(pPlugin != NULL);

  if(pPlugin == NULL) 
  {
    dbgOut1("NPP_SetWindow returns NPERR_GENERIC_ERROR");
    return NPERR_GENERIC_ERROR;
  }

  if((hWnd == NULL) && (pPlugin->getWindow() == NULL)) // spurious entry
  {
    dbgOut1("NPP_SetWindow just returns with NPERR_NO_ERROR");
    return NPERR_NO_ERROR;
  }

  if((hWnd == NULL) && (pPlugin->getWindow() != NULL))
  { // window went away
    dbgOut1("NPP_SetWindow, going away...");
    pPlugin->shut();
    return NPERR_NO_ERROR;
  }

  if((pPlugin->getWindow() == NULL) && (hWnd != NULL))
  { // First time in -- no window created by plugin yet
    dbgOut1("NPP_SetWindow, first time");

    if(!pPlugin->init(hWnd))
    {
      delete pPlugin;
      pPlugin = NULL;
      return NPERR_MODULE_LOAD_FAILED_ERROR;
    }
  }

  if((pPlugin->getWindow() != NULL) && (hWnd != NULL))
  { // Netscape window has been resized
    dbgOut1("NPP_SetWindow, resizing");
    pPlugin->resize();
  }

  return NPERR_NO_ERROR;
}

//------------------------------------------------------------------------------------
// NPP_NewStream:
//------------------------------------------------------------------------------------
NPError NP_LOADDS
NPP_NewStream(NPP pInstance,
              NPMIMEType type,
              NPStream *stream, 
              NPBool seekable,
              uint16 *stype)
{
  dbgOut1("NPP_NewStream");
  if(pInstance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  CPlugin * pPlugin = (CPlugin *)pInstance->pdata;
  assert(pPlugin != NULL);

  return NPERR_NO_ERROR;
}

//------------------------------------------------------------------------------------
// NPP_WriteReady:
//------------------------------------------------------------------------------------
int32 NP_LOADDS
NPP_WriteReady(NPP pInstance, NPStream *stream)
{
  dbgOut1("NPP_WriteReady");
  if(pInstance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  CPlugin * pPlugin = (CPlugin *)pInstance->pdata;
  assert(pPlugin != NULL);

  return -1L;   // dont accept any bytes in NPP_Write()
}

//------------------------------------------------------------------------------------
// NPP_Write:
//------------------------------------------------------------------------------------
int32 NP_LOADDS
NPP_Write(NPP pInstance, NPStream *stream, int32 offset, int32 len, void *buffer)
{
  //dbgOut1("NPP_Write");
  if(pInstance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  CPlugin * pPlugin = (CPlugin *)pInstance->pdata;
  assert(pPlugin != NULL);

  return -1;   // tell Nav to abort the stream, don't need it
}

//------------------------------------------------------------------------------------
// NPP_DestroyStream:
//------------------------------------------------------------------------------------
NPError NP_LOADDS
NPP_DestroyStream(NPP pInstance, NPStream *stream, NPError reason)
{
  dbgOut1("NPP_DestroyStream");
  if(pInstance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  CPlugin * pPlugin = (CPlugin *)pInstance->pdata;
  assert(pPlugin != NULL);

  return NPERR_NO_ERROR;
}

//------------------------------------------------------------------------------------
// NPP_StreamAsFile:
//------------------------------------------------------------------------------------
void NP_LOADDS
NPP_StreamAsFile(NPP instance, NPStream *stream, const char* fname)
{
  dbgOut1("NPP_StreamAsFile");
}

//------------------------------------------------------------------------------------
// NPP_Print:
//------------------------------------------------------------------------------------
void NP_LOADDS NPP_Print(NPP pInstance, NPPrint * printInfo)
{
  dbgOut2("NPP_Print, printInfo = %#08x", printInfo);

  CPlugin * pPlugin = (CPlugin *)pInstance->pdata;
  assert(pPlugin != NULL);

  pPlugin->print(printInfo);
}

void NP_LOADDS NPP_URLNotify(NPP pInstance, const char* url, NPReason reason, void* notifyData)
{
  dbgOut2("NPP_URLNotify, URL '%s'", url);

  CPlugin * pPlugin = (CPlugin *)pInstance->pdata;
  assert(pPlugin != NULL);

  pPlugin->URLNotify(url);
}

jref NP_LOADDS NPP_GetJavaClass(void)
{
  return NULL;
}
