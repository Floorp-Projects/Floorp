/* ***** BEGIN LICENSE BLOCK *****
 * 
 * Copyright (c) 2008, Mozilla Corporation
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of the Mozilla Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * Contributor(s):
 *   Josh Aas <josh@mozilla.com>
 * 
 * ***** END LICENSE BLOCK ***** */

#include "nptest_platform.h"
#include <CoreServices/CoreServices.h>

NPError
pluginInstanceInit(InstanceData* instanceData)
{
  NPP npp = instanceData->npp;
  // select the right drawing model if necessary
  NPBool supportsCoreGraphics = false;
  if (NPN_GetValue(npp, NPNVsupportsCoreGraphicsBool, &supportsCoreGraphics) == NPERR_NO_ERROR && supportsCoreGraphics) {
    NPN_SetValue(npp, NPPVpluginDrawingModel, (void*)NPDrawingModelCoreGraphics);
  } else {
    printf("CoreGraphics drawing model not supported, can't create a plugin instance.\n");
    return NPERR_INCOMPATIBLE_VERSION_ERROR;
  }
  return NPERR_NO_ERROR;
}

int16_t
pluginHandleEvent(InstanceData* instanceData, void* event)
{
  EventRecord* carbonEvent = (EventRecord*)event;
  if (carbonEvent && (carbonEvent->what == updateEvt)) {
    pluginDraw(instanceData);
    return 1;
  }
  return 0;
}

void
pluginDraw(InstanceData* instanceData)
{
  if (!instanceData)
    return;

  NPP npp = instanceData->npp;
  if (!npp)
    return;

  const char* uaString = NPN_UserAgent(npp);
  if (!uaString)
    return;

  NPWindow window = instanceData->window;

  CGContextRef cgContext = ((NP_CGContext*)(window.window))->context;

  CFStringRef uaCFString = CFStringCreateWithCString(kCFAllocatorDefault, uaString, kCFStringEncodingASCII);

  float windowWidth = window.width;
  float windowHeight = window.height;

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
  CFIndex stringLength = CFStringGetLength(uaCFString);
  UniChar* unicharBuffer = (UniChar*)malloc((stringLength + 1) * sizeof(UniChar));
  CFStringGetCharacters(uaCFString, CFRangeMake(0, stringLength), unicharBuffer);
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
  unsigned int i = 0;
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
