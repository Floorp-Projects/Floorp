/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Josh Aas <josh@mozilla.com>
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

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#pragma GCC visibility push(default)
extern "C"
{
NPError NP_Initialize(NPNetscapeFuncs *browserFuncs);
NPError NP_GetEntryPoints(NPPluginFuncs *pluginFuncs);
void    NP_Shutdown(void);

NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16_t mode,
                int16_t argc, char* argn[], char* argv[], NPSavedData* saved);
NPError NPP_Destroy(NPP instance, NPSavedData** save);
NPError NPP_SetWindow(NPP instance, NPWindow* window);
NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype);
NPError NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason);
int32_t NPP_WriteReady(NPP instance, NPStream* stream);
int32_t NPP_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len, void* buffer);
void    NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname);
void    NPP_Print(NPP instance, NPPrint* platformPrint);
int16_t NPP_HandleEvent(NPP instance, void* event);
void    NPP_URLNotify(NPP instance, const char* URL, NPReason reason, void* notifyData);
NPError NPP_GetValue(NPP instance, NPPVariable variable, void *value);
NPError NPP_SetValue(NPP instance, NPNVariable variable, void *value);
}
#pragma GCC visibility pop

// structure containing pointers to functions implemented by the browser
static NPNetscapeFuncs* browser;

// data for each instance of this plugin
typedef struct PluginInstance {
  NPP npp;
  NPWindow window;
} PluginInstance;

void drawPlugin(NPP instance, NPCocoaEvent* event);

// Symbol called once by the browser to initialize the plugin
NPError NP_Initialize(NPNetscapeFuncs* browserFuncs)
{
  // save away browser functions
  browser = browserFuncs;

  return NPERR_NO_ERROR;
}

// Symbol called by the browser to get the plugin's function list
NPError NP_GetEntryPoints(NPPluginFuncs* pluginFuncs)
{
  pluginFuncs->version = 11;
  pluginFuncs->size = sizeof(pluginFuncs);
  pluginFuncs->newp = NPP_New;
  pluginFuncs->destroy = NPP_Destroy;
  pluginFuncs->setwindow = NPP_SetWindow;
  pluginFuncs->newstream = NPP_NewStream;
  pluginFuncs->destroystream = NPP_DestroyStream;
  pluginFuncs->asfile = NPP_StreamAsFile;
  pluginFuncs->writeready = NPP_WriteReady;
  pluginFuncs->write = (NPP_WriteProcPtr)NPP_Write;
  pluginFuncs->print = NPP_Print;
  pluginFuncs->event = NPP_HandleEvent;
  pluginFuncs->urlnotify = NPP_URLNotify;
  pluginFuncs->getvalue = NPP_GetValue;
  pluginFuncs->setvalue = NPP_SetValue;

  return NPERR_NO_ERROR;
}

// Symbol called once by the browser to shut down the plugin
void NP_Shutdown(void)
{
  
}

// Called to create a new instance of the plugin
NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16_t mode,
                int16_t argc, char* argn[], char* argv[], NPSavedData* saved)
{
  PluginInstance *newInstance = (PluginInstance*)malloc(sizeof(PluginInstance));
  bzero(newInstance, sizeof(PluginInstance));
  
  newInstance->npp = instance;
  instance->pdata = newInstance;

  // select the right drawing model if necessary
  NPBool supportsCoreGraphics = false;
  if ((browser->getvalue(instance, NPNVsupportsCoreGraphicsBool, &supportsCoreGraphics) == NPERR_NO_ERROR) &&
      supportsCoreGraphics) {
    browser->setvalue(instance, NPPVpluginDrawingModel, (void*)NPDrawingModelCoreGraphics);
  } else {
    printf("CoreGraphics drawing model not supported, can't create a plugin instance.\n");
    return NPERR_INCOMPATIBLE_VERSION_ERROR;
  }

  // select the Cocoa event model
  NPBool supportsCocoaEvents = false;
  if ((browser->getvalue(instance, NPNVsupportsCocoaBool, &supportsCocoaEvents) == NPERR_NO_ERROR) &&
      supportsCocoaEvents) {
    browser->setvalue(instance, NPPVpluginEventModel, (void*)NPEventModelCocoa);
  } else {
    printf("Cocoa event model not supported, can't create a plugin instance.\n");
    return NPERR_INCOMPATIBLE_VERSION_ERROR;
  }

  return NPERR_NO_ERROR;
}

// Called to destroy an instance of the plugin
NPError NPP_Destroy(NPP instance, NPSavedData** save)
{
  free(instance->pdata);
  
  return NPERR_NO_ERROR;
}

// Called to update a plugin instances's NPWindow
NPError NPP_SetWindow(NPP instance, NPWindow* window)
{
  PluginInstance* currentInstance = (PluginInstance*)(instance->pdata);
  
  currentInstance->window = *window;
  
  return NPERR_NO_ERROR;
}


NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype)
{
  *stype = NP_ASFILEONLY;
  return NPERR_NO_ERROR;
}

NPError NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason)
{
  return NPERR_NO_ERROR;
}

int32_t NPP_WriteReady(NPP instance, NPStream* stream)
{
  return 0;
}

int32_t NPP_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len, void* buffer)
{
  return 0;
}

void NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname)
{
  
}

void NPP_Print(NPP instance, NPPrint* platformPrint)
{
  
}

int16_t NPP_HandleEvent(NPP instance, void* event)
{
  NPCocoaEvent* cocoaEvent = (NPCocoaEvent*)event;
  if (cocoaEvent && (cocoaEvent->type == NPCocoaEventDrawRect)) {
    drawPlugin(instance, (NPCocoaEvent*)event);
    return 1;
  }

  return 0;
}

void NPP_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData)
{
  
}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void *value)
{
  return NPERR_GENERIC_ERROR;
}

NPError NPP_SetValue(NPP instance, NPNVariable variable, void *value)
{
  return NPERR_GENERIC_ERROR;
}

void drawPlugin(NPP instance, NPCocoaEvent* event)
{
  PluginInstance* currentInstance = (PluginInstance*)(instance->pdata);
  CGContextRef cgContext = event->data.draw.context;
  if (cgContext)
    return;

  float windowWidth = currentInstance->window.width;
  float windowHeight = currentInstance->window.height;

  // save the cgcontext gstate
  CGContextSaveGState(cgContext);

  // we get a flipped context
  CGContextTranslateCTM(cgContext, 0.0, windowHeight);
  CGContextScaleCTM(cgContext, 1.0, -1.0);

  // draw a white background for the plugin
  CGContextAddRect(cgContext, CGRectMake(0, 0, windowWidth, windowHeight));
  CGContextSetGrayFillColor(cgContext, 1.0, 1.0);
  CGContextDrawPath(cgContext, kCGPathFill);

  // draw the broken plugin icon
  CFBundleRef bundle = ::CFBundleGetBundleWithIdentifier(CFSTR("org.mozilla.DefaultPlugin"));
  CFURLRef imageURL = ::CFBundleCopyResourceURL(bundle, CFSTR("plugin"), CFSTR("png"), NULL);
  CGDataProviderRef dataProvider = ::CGDataProviderCreateWithURL(imageURL);
  ::CFRelease(imageURL);
  CGImageRef imageRef = ::CGImageCreateWithPNGDataProvider(dataProvider, NULL, TRUE,
                                                           kCGRenderingIntentDefault);
  ::CGDataProviderRelease(dataProvider);
  float imageWidth = ::CGImageGetWidth(imageRef);
  float imageHeight = ::CGImageGetHeight(imageRef);
  CGRect drawRect = ::CGRectMake(windowWidth / 2 - imageWidth / 2,
                                 windowHeight / 2 - imageHeight / 2,
                                 imageWidth,
                                 imageHeight);
  ::CGContextDrawImage(cgContext, drawRect, imageRef);
  ::CGImageRelease(imageRef);

  // draw a blue frame around the plugin
  CGContextAddRect(cgContext, CGRectMake(0, 0, windowWidth, windowHeight));
  CGContextSetRGBStrokeColor(cgContext, 0.0, 0.0, 0.5, 1.0);
  CGContextSetLineWidth(cgContext, 2.0);
  CGContextStrokePath(cgContext);

  // restore the cgcontext gstate
  CGContextRestoreGState(cgContext);
}
