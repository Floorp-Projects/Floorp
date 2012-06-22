/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <cassert>
#if defined(_WIN32)
#include <conio.h>
#endif

#include "voe_cpu_test.h"

using namespace webrtc;

namespace voetest {

#define CHECK(expr)                                             \
    if (expr)                                                   \
    {                                                           \
        printf("Error at line: %i, %s \n", __LINE__, #expr);    \
        printf("Error code: %i \n", base->LastError());  \
        PAUSE												    \
        return -1;                                              \
    }

extern char* GetFilename(char* filename);
extern const char* GetFilename(const char* filename);
extern int GetResource(char* resource, char* dest, int destLen);
extern char* GetResource(char* resource);
extern const char* GetResource(const char* resource);

VoECpuTest::VoECpuTest(VoETestManager& mgr)
    : _mgr(mgr) {

}

int VoECpuTest::DoTest() {
  printf("------------------------------------------------\n");
  printf(" CPU Reference Test\n");
  printf("------------------------------------------------\n");

  VoEBase* base = _mgr.BasePtr();
  VoEFile* file = _mgr.FilePtr();
  VoECodec* codec = _mgr.CodecPtr();
  VoEAudioProcessing* apm = _mgr.APMPtr();

  int channel(-1);
  CodecInst isac;

  isac.pltype = 104;
  strcpy(isac.plname, "ISAC");
  isac.pacsize = 960;
  isac.plfreq = 32000;
  isac.channels = 1;
  isac.rate = -1;

  CHECK(base->Init());
  channel = base->CreateChannel();

  CHECK(base->SetLocalReceiver(channel, 5566));
  CHECK(base->SetSendDestination(channel, 5566, "127.0.0.1"));
  CHECK(codec->SetRecPayloadType(channel, isac));
  CHECK(codec->SetSendCodec(channel, isac));

  CHECK(base->StartReceive(channel));
  CHECK(base->StartPlayout(channel));
  CHECK(base->StartSend(channel));
  CHECK(file->StartPlayingFileAsMicrophone(channel, _mgr.AudioFilename(),
          true, true));

  CHECK(codec->SetVADStatus(channel, true));
  CHECK(apm->SetAgcStatus(true, kAgcAdaptiveAnalog));
  CHECK(apm->SetNsStatus(true, kNsModerateSuppression));
  CHECK(apm->SetEcStatus(true, kEcAec));

  TEST_LOG("\nMeasure CPU and memory while running a full-duplex"
    " iSAC-swb call.\n\n");

  PAUSE

  CHECK(base->StopSend(channel));
  CHECK(base->StopPlayout(channel));
  CHECK(base->StopReceive(channel));

  base->DeleteChannel(channel);
  CHECK(base->Terminate());

  return 0;
}

} //  namespace voetest
