/* ***** BEGIN LICENSE BLOCK *****
 *
 * THIS FILE IS PART OF THE MOZILLA NPAPI SDK BASIC PLUGIN SAMPLE
 * SOURCE CODE. USE, DISTRIBUTION AND REPRODUCTION OF THIS SOURCE
 * IS GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS 
 * SOURCE IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.
 *
 * THE MOZILLA NPAPI SDK BASIC PLUGIN SAMPLE SOURCE CODE IS
 * (C) COPYRIGHT 2008 by the Mozilla Corporation
 * http://www.mozilla.com/
 *
 * Contributors:
 *  Josh Aas <josh@mozilla.com>
 *
 * ***** END LICENSE BLOCK ***** */

#include "BasicPlugin.h"

// structure containing pointers to functions implemented by the browser
static NPNetscapeFuncs* browser;

// local store of the browser UA string that we we paint into the plugin's window
static CFStringRef browserUAString = NULL;

// data for each instance of this plugin
typedef struct PluginInstance {
  NPP npp;
  NPWindow window;
} PluginInstance;

void drawPlugin(NPP instance);

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
  CFRelease(browserUAString);
  browserUAString = NULL;
}

// Called to create a new instance of the plugin
NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char* argn[], char* argv[], NPSavedData* saved)
{
  PluginInstance *newInstance = (PluginInstance*)malloc(sizeof(PluginInstance));
  bzero(newInstance, sizeof(PluginInstance));

  newInstance->npp = instance;
  instance->pdata = newInstance;

  NPBool supportsCoreGraphics;
  if (browser->getvalue(instance, NPNVsupportsCoreGraphicsBool, &supportsCoreGraphics) != NPERR_NO_ERROR)
    supportsCoreGraphics = FALSE;

  if (!supportsCoreGraphics)
    return NPERR_INCOMPATIBLE_VERSION_ERROR;

  browser->setvalue(instance, NPPVpluginDrawingModel, (void *)NPDrawingModelCoreGraphics);

  if (!browserUAString) {
    const char* ua = browser->uagent(instance);
    if (ua)
      browserUAString = CFStringCreateWithCString(kCFAllocatorDefault, ua, kCFStringEncodingASCII);
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
  EventRecord* carbonEvent = (EventRecord*)event;
	if (carbonEvent && (carbonEvent->what == updateEvt))
    drawPlugin(instance);

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

void drawPlugin(NPP instance)
{
  if (!browserUAString)
    return;

  PluginInstance* currentInstance = (PluginInstance*)(instance->pdata);
  CGContextRef cgContext = ((NP_CGContext*)(currentInstance->window.window))->context;
  
  float windowWidth = currentInstance->window.width;
  float windowHeight = currentInstance->window.height;
  
  // save the cgcontext gstate
  CGContextSaveGState(cgContext);
  
  // we get a flipped context
  CGContextTranslateCTM(cgContext, 0.0, windowHeight);
  CGContextScaleCTM(cgContext, 1.0, -1.0);
  
  // draw a gray background for the plugin
  CGContextAddRect(cgContext, CGRectMake(0, 0, windowWidth, windowHeight));
  CGContextSetGrayFillColor(cgContext, 0.5, 1.0);
  CGContextDrawPath(cgContext, kCGPathFill);
  
  // draw a black frame around the plugin
  CGContextAddRect(cgContext, CGRectMake(0, 0, windowWidth, windowHeight));
  CGContextSetGrayStrokeColor(cgContext, 0.0, 1.0);
  CGContextSetLineWidth(cgContext, 6.0);
  CGContextStrokePath(cgContext);
  
  // draw the UA string using ATSUI
  CGContextSetGrayFillColor(cgContext, 0.0, 1.0);
  ATSUStyle atsuStyle;
  ATSUCreateStyle(&atsuStyle);
  CFIndex stringLength = CFStringGetLength(browserUAString);
  UniChar* unicharBuffer = (UniChar*)malloc((stringLength + 1) * sizeof(UniChar));
  CFStringGetCharacters(browserUAString, CFRangeMake(0, stringLength), unicharBuffer);
  UniCharCount runLengths = kATSUToTextEnd;
  ATSUTextLayout atsuLayout;
  ATSUCreateTextLayoutWithTextPtr(unicharBuffer,
                                  kATSUFromTextBeginning,
                                  kATSUToTextEnd,
                                  stringLength,
                                  1,
                                  &runLengths,
                                  &atsuStyle,
                                  &atsuLayout);
  ATSUAttributeTag contextTag = kATSUCGContextTag;
  ByteCount byteSize = sizeof(CGContextRef);
  ATSUAttributeValuePtr contextATSUPtr = &cgContext;
  ATSUSetLayoutControls(atsuLayout, 1, &contextTag, &byteSize, &contextATSUPtr);
  ATSUTextMeasurement lineAscent, lineDescent;
  ATSUGetLineControl(atsuLayout,
                    kATSUFromTextBeginning,
                    kATSULineAscentTag,
                    sizeof(ATSUTextMeasurement),
                    &lineAscent,
                    &byteSize);
  ATSUGetLineControl(atsuLayout,
                    kATSUFromTextBeginning,
                    kATSULineDescentTag,
                    sizeof(ATSUTextMeasurement),
                    &lineDescent,
                    &byteSize);
  float lineHeight = FixedToFloat(lineAscent) + FixedToFloat(lineDescent);  
  ItemCount softBreakCount;
  ATSUBatchBreakLines(atsuLayout,
                      kATSUFromTextBeginning,
                      stringLength,
                      FloatToFixed(windowWidth - 10.0),
                      &softBreakCount);
  ATSUGetSoftLineBreaks(atsuLayout,
                        kATSUFromTextBeginning,
                        kATSUToTextEnd,
                        0, NULL, &softBreakCount);
  UniCharArrayOffset* softBreaks = (UniCharArrayOffset*)malloc(softBreakCount * sizeof(UniCharArrayOffset));
  ATSUGetSoftLineBreaks(atsuLayout,
                        kATSUFromTextBeginning,
                        kATSUToTextEnd,
                        softBreakCount, softBreaks, &softBreakCount);
  UniCharArrayOffset currentDrawOffset = kATSUFromTextBeginning;
  int i = 0;
  while (i < softBreakCount) {
    ATSUDrawText(atsuLayout, currentDrawOffset, softBreaks[i], FloatToFixed(5.0), FloatToFixed(windowHeight - 5.0 - (lineHeight * (i + 1.0))));
    currentDrawOffset = softBreaks[i];
    i++;
  }
  ATSUDrawText(atsuLayout, currentDrawOffset, kATSUToTextEnd, FloatToFixed(5.0), FloatToFixed(windowHeight - 5.0 - (lineHeight * (i + 1.0))));
  free(unicharBuffer);
  free(softBreaks);
  
  // restore the cgcontext gstate
  CGContextRestoreGState(cgContext);
}
