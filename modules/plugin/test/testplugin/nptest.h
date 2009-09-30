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

#ifndef nptest_h_
#define nptest_h_

#include "mozilla-config.h"

#include "npapi.h"
#include "npfunctions.h"
#include "npruntime.h"
#include "prtypes.h"
#include <string>
#include <sstream>

using namespace std;

typedef enum  {
  DM_DEFAULT,
  DM_SOLID_COLOR
} DrawMode;

typedef enum {
  FUNCTION_NONE,
  FUNCTION_NPP_GETURL,
  FUNCTION_NPP_GETURLNOTIFY,
  FUNCTION_NPP_POSTURL,
  FUNCTION_NPP_POSTURLNOTIFY
} TestFunction;

typedef enum {
  POSTMODE_FRAME,
  POSTMODE_STREAM
} PostMode;

typedef struct TestNPObject : NPObject {
  NPP npp;
  DrawMode drawMode;
  PRUint32 drawColor; // 0xAARRGGBB
} TestNPObject;

typedef struct _PlatformData PlatformData;

typedef struct TestRange : NPByteRange {
  bool waiting;
} TestRange;

typedef struct InstanceData {
  NPP npp;
  NPWindow window;
  TestNPObject* scriptableObject;
  PlatformData* platformData;
  uint32_t instanceCountWatchGeneration;
  bool lastReportedPrivateModeState;
  bool hasWidget;
  bool npnNewStream;
  uint32_t timerID1;
  uint32_t timerID2;
  int32_t lastMouseX;
  int32_t lastMouseY;
  TestFunction testFunction;
  PostMode postMode;
  string testUrl;
  string frame;
  ostringstream err;
  uint16_t streamMode;
  int32_t streamChunkSize;
  int32_t streamBufSize;
  int32_t fileBufSize;
  TestRange* testrange;
  void* streamBuf;
  void* fileBuf;
} InstanceData;

#endif // nptest_h_
