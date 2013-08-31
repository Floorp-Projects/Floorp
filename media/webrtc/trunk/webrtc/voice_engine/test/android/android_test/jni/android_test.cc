/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <android/log.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "webrtc/voice_engine/test/android/android_test/jni/org_webrtc_voiceengine_test_AndroidTest.h"

#include "webrtc/system_wrappers/interface/thread_wrapper.h"

#include "webrtc/voice_engine/include/voe_audio_processing.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_codec.h"
#include "webrtc/voice_engine/include/voe_encryption.h"
#include "webrtc/voice_engine/include/voe_file.h"
#include "webrtc/voice_engine/include/voe_hardware.h"
#include "webrtc/voice_engine/include/voe_network.h"
#include "webrtc/voice_engine/include/voe_rtp_rtcp.h"
#include "webrtc/voice_engine/include/voe_volume_control.h"

#include "webrtc/voice_engine/test/auto_test/voe_test_interface.h"

//#define INIT_FROM_THREAD
//#define START_CALL_FROM_THREAD

#define WEBRTC_LOG_TAG "*WEBRTCN*" // As in WEBRTC Native...
#define VALIDATE_BASE_POINTER \
    if (!veData1.base) \
    { \
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG, \
                            "Base pointer doesn't exist"); \
        return -1; \
    }
#define VALIDATE_CODEC_POINTER \
    if (!veData1.codec) \
    { \
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG, \
                            "Codec pointer doesn't exist"); \
        return -1; \
    }
#define VALIDATE_FILE_POINTER \
    if (!veData1.file) \
    { \
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG, \
                            "File pointer doesn't exist"); \
        return -1; \
    }
#define VALIDATE_NETWORK_POINTER \
    if (!veData1.netw) \
    { \
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG, \
                            "Network pointer doesn't exist"); \
        return -1; \
    }
#define VALIDATE_APM_POINTER \
    if (!veData1.codec) \
    { \
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG, \
                            "Apm pointer doesn't exist"); \
        return -1; \
    }
#define VALIDATE_VOLUME_POINTER \
    if (!veData1.volume) \
    { \
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG, \
                            "Volume pointer doesn't exist"); \
        return -1; \
    }
#define VALIDATE_HARDWARE_POINTER \
    if (!veData1.hardware) \
    { \
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG, \
                            "Hardware pointer doesn't exist"); \
        return -1; \
    }
#define VALIDATE_RTP_RTCP_POINTER \
    if (!veData1.rtp_rtcp) \
    { \
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG, \
                            "RTP / RTCP pointer doesn't exist"); \
        return -1; \
    }
#define VALIDATE_ENCRYPT_POINTER \
    if (!veData1.encrypt) \
    { \
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG, \
                            "Encrypt pointer doesn't exist"); \
        return -1; \
    }

// Register functions in JNI_OnLoad()
// How do we ensure that VoE is deleted? JNI_OnUnload?
// What happens if class is unloaded? When loaded again, NativeInit will be
// called again. Keep what we have?
// Should we do something in JNI_OnUnload?
// General design: create a class or keep global struct with "C" functions?
// Otherwise make sure symbols are as unique as possible.

// TestType enumerator
enum TestType
{
  Invalid = -1,
  Standard = 0,
  Extended = 1,
  Stress   = 2,
  Unit     = 3,
  CPU      = 4
};

// ExtendedSelection enumerator
enum ExtendedSelection
{
   XSEL_Invalid = -1,
   XSEL_None = 0,
   XSEL_All,
   XSEL_Base,
   XSEL_CallReport,
   XSEL_Codec,
   XSEL_DTMF,
   XSEL_Encryption,
   XSEL_ExternalMedia,
   XSEL_File,
   XSEL_Hardware,
   XSEL_NetEqStats,
   XSEL_Network,
   XSEL_PTT,
   XSEL_RTP_RTCP,
   XSEL_VideoSync,
   XSEL_VideoSyncExtended,
   XSEL_VolumeControl,
   XSEL_VQE,
   XSEL_APM,
   XSEL_VQMon
};

using namespace webrtc;

class my_transportation;

// VoiceEngine data struct
typedef struct
{
    // VoiceEngine
    VoiceEngine* ve;
    // Sub-APIs
    VoEBase* base;
    VoECodec* codec;
    VoEFile* file;
    VoENetwork* netw;
    VoEAudioProcessing* apm;
    VoEVolumeControl* volume;
    VoEHardware* hardware;
    VoERTP_RTCP* rtp_rtcp;
    VoEEncryption* encrypt;
    // Other
    my_transportation* extTrans;
    JavaVM* jvm;
} VoiceEngineData;

// my_transportation is used when useExtTrans is enabled
class my_transportation : public Transport
{
 public:
  my_transportation(VoENetwork * network) :
      netw(network) {
  }

  int SendPacket(int channel,const void *data,int len);
  int SendRTCPPacket(int channel, const void *data, int len);
 private:
  VoENetwork * netw;
};

int my_transportation::SendPacket(int channel,const void *data,int len)
{
  netw->ReceivedRTPPacket(channel, data, len);
  return len;
}

int my_transportation::SendRTCPPacket(int channel, const void *data, int len)
{
  netw->ReceivedRTCPPacket(channel, data, len);
  return len;
}

//Global variables visible in this file
static VoiceEngineData veData1;
static VoiceEngineData veData2;

// "Local" functions (i.e. not Java accessible)
static bool GetSubApis(VoiceEngineData &veData);
static bool ReleaseSubApis(VoiceEngineData &veData);

class ThreadTest
{
public:
    ThreadTest();
    ~ThreadTest();
    int RunTest();
    int CloseTest();
private:
    static bool Run(void* ptr);
    bool Process();
private:
    ThreadWrapper* _thread;
};

ThreadTest::~ThreadTest()
{
    if (_thread)
    {
        _thread->SetNotAlive();
        if (_thread->Stop())
        {
            delete _thread;
            _thread = NULL;
        }
    }
}

ThreadTest::ThreadTest() :
    _thread(NULL)
{
    _thread = ThreadWrapper::CreateThread(Run, this, kNormalPriority,
                                          "ThreadTest thread");
}

bool ThreadTest::Run(void* ptr)
{
    return static_cast<ThreadTest*> (ptr)->Process();
}

bool ThreadTest::Process()
{
    // Attach this thread to JVM
    /*JNIEnv* env = NULL;
     jint res = veData1.jvm->AttachCurrentThread(&env, NULL);
     char msg[32];
     sprintf(msg, "res=%d, env=%d", res, env);
     __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG, msg);*/

#ifdef INIT_FROM_THREAD
    VALIDATE_BASE_POINTER;
    veData1.base->Init();
#endif

#ifdef START_CALL_FROM_THREAD
    // receiving instance
    veData2.ve = VoiceEngine::Create();
    GetSubApis(veData2);
    veData2.base->Init();
    veData2.base->CreateChannel();
    if(veData2.base->SetLocalReceiver(0, 1234) < 0)
    {
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                "set local receiver 2 failed");
    }
    veData2.hardware->SetLoudspeakerStatus(false);
    veData2.volume->SetSpeakerVolume(204);
    veData2.base->StartReceive(0);
    if(veData2.base->StartPlayout(0) < 0)
    {
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                "start playout failed");
    }

    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
            "receiving instance started from thread");

    // sending instance
    veData1.ve = VoiceEngine::Create();
    GetSubApis(veData1);
    veData1.base->Init();
    if(veData1.base->CreateChannel() < 0)
    {
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                "create channel failed");
    }
    if(veData1.base->SetLocalReceiver(0, 1256) < 0)
    {
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                "set local receiver failed");
    }
    if(veData1.base->SetSendDestination(0, 1234, "127.0.0.1") < 0)
    {
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                "set send destination failed");
    }
    if(veData1.base->StartSend(0) < 0)
    {
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                "start send failed");
    }

    __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
            "sending instance started from thread");
#endif

    _thread->SetNotAlive();
    _thread->Stop();

    //res = veData1.jvm->DetachCurrentThread();

    return true;
}

int ThreadTest::RunTest()
{
    if (_thread)
    {
        unsigned int id;
        _thread->Start(id);
    }
    return 0;
}

int ThreadTest::CloseTest()
{
    VALIDATE_BASE_POINTER

    veData1.base->DeleteChannel(0);
    veData2.base->DeleteChannel(0);
    veData1.base->Terminate();
    veData2.base->Terminate();

    // Release sub-APIs
    ReleaseSubApis(veData1);
    ReleaseSubApis(veData2);

    // Delete
    VoiceEngine::Delete(veData1.ve);
    VoiceEngine::Delete(veData2.ve);
    veData2.ve = NULL;
    veData2.ve = NULL;

    return 0;
}

ThreadTest threadTest;

//////////////////////////////////////////////////////////////////
// General functions
//////////////////////////////////////////////////////////////////

/////////////////////////////////////////////
// JNI_OnLoad
//
jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/)
{
    if (!vm)
    {
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                            "JNI_OnLoad did not receive a valid VM pointer");
        return -1;
    }

    // Get JNI
    JNIEnv* env;
    if (JNI_OK != vm->GetEnv(reinterpret_cast<void**> (&env),
                             JNI_VERSION_1_4))
    {
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                            "JNI_OnLoad could not get JNI env");
        return -1;
    }

    // Get class to register the native functions with
    // jclass regClass = env->FindClass("webrtc/android/AndroidTest");
    // if (!regClass) {
    // return -1; // Exception thrown
    // }

    // Register native functions
    // JNINativeMethod methods[1];
    // methods[0].name = NULL;
    // methods[0].signature = NULL;
    // methods[0].fnPtr = NULL;
    // if (JNI_OK != env->RegisterNatives(regClass, methods, 1))
    // {
    // return -1;
    // }

    // Init VoiceEngine data
    memset(&veData1, 0, sizeof(veData1));
    memset(&veData2, 0, sizeof(veData2));

    // Store the JVM
    veData1.jvm = vm;
    veData2.jvm = vm;

    return JNI_VERSION_1_4;
}

/////////////////////////////////////////////
// Native initialization
//
JNIEXPORT jboolean JNICALL
Java_org_webrtc_voiceengine_test_AndroidTest_NativeInit(
        JNIEnv * env,
        jclass)
{
    // Look up and cache any interesting class, field and method IDs for
    // any used java class here

    return true;
}

/////////////////////////////////////////////
// Run auto standard test
//
JNIEXPORT jint JNICALL Java_org_webrtc_voiceengine_test_AndroidTest_RunAutoTest(
        JNIEnv *env,
        jobject context,
        jint testType,
        jint extendedSel)
{
    TestType tType(Invalid);

    switch (testType)
    {
        case 0:
            return 0;
        case 1:
            tType = Standard;
            break;
        case 2:
            tType = Extended;
            break;
        case 3:
            tType = Stress;
            break;
        case 4:
            tType = Unit;
            break;
        default:
            __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                                "RunAutoTest - Invalid TestType");
            return -1;
    }

    ExtendedSelection xsel(XSEL_Invalid);

    switch (extendedSel)
    {
        case 0:
            xsel = XSEL_None;
            break;
        case 1:
            xsel = XSEL_All;
            break;
        case 2:
            xsel = XSEL_Base;
            break;
        case 3:
            xsel = XSEL_CallReport;
            break;
        case 4:
            xsel = XSEL_Codec;
            break;
        case 5:
            xsel = XSEL_DTMF;
            break;
        case 6:
            xsel = XSEL_Encryption;
            break;
        case 7:
            xsel = XSEL_ExternalMedia;
            break;
        case 8:
            xsel = XSEL_File;
            break;
        case 9:
            xsel = XSEL_Hardware;
            break;
        case 10:
            xsel = XSEL_NetEqStats;
            break;
        case 11:
            xsel = XSEL_Network;
            break;
        case 12:
            xsel = XSEL_PTT;
            break;
        case 13:
            xsel = XSEL_RTP_RTCP;
            break;
        case 14:
            xsel = XSEL_VideoSync;
            break;
        case 15:
            xsel = XSEL_VideoSyncExtended;
            break;
        case 16:
            xsel = XSEL_VolumeControl;
            break;
        case 17:
            xsel = XSEL_APM;
            break;
        case 18:
            xsel = XSEL_VQMon;
            break;
        default:
            xsel = XSEL_Invalid;
            __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                                "RunAutoTest - Invalid extendedType");
            return -1;
    }

    // Set instance independent Java objects
    VoiceEngine::SetAndroidObjects(veData1.jvm, env, context);

    // Call voe test interface function
    // TODO(leozwang) add autotest setAndroidObjects(veData1.jvm, context);
    // jint retVal = runAutoTest(tType, xsel);

    // Clear instance independent Java objects
    VoiceEngine::SetAndroidObjects(NULL, NULL, NULL);

    return 0;
}

//////////////////////////////////////////////////////////////////
// VoiceEngine API wrapper functions
//////////////////////////////////////////////////////////////////

/////////////////////////////////////////////
// Create VoiceEngine instance
//
JNIEXPORT jboolean JNICALL Java_org_webrtc_voiceengine_test_AndroidTest_Create(
        JNIEnv *env,
        jobject context)
{
    // Check if already created
    if (veData1.ve)
    {
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                            "VoE already created");
        return false;
    }

    // Set instance independent Java objects
    VoiceEngine::SetAndroidObjects(veData1.jvm, env, context);

#ifdef START_CALL_FROM_THREAD
    threadTest.RunTest();
#else
    // Create
    veData1.ve = VoiceEngine::Create();
    if (!veData1.ve)
    {
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                            "Create VoE failed");
        return false;
    }

    // Get sub-APIs
    if (!GetSubApis(veData1))
    {
        // If not OK, release all sub-APIs and delete VoE
        ReleaseSubApis(veData1);
        if (!VoiceEngine::Delete(veData1.ve))
        {
            __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                                "Delete VoE failed");
        }
        return false;
    }
#endif

    return true;
}

/////////////////////////////////////////////
// Delete VoiceEngine instance
//
JNIEXPORT jboolean JNICALL Java_org_webrtc_voiceengine_test_AndroidTest_Delete(
        JNIEnv *,
        jobject)
{
#ifdef START_CALL_FROM_THREAD
    threadTest.CloseTest();
#else
    // Check if exists
    if (!veData1.ve)
    {
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                            "VoE does not exist");
        return false;
    }

    // Release sub-APIs
    ReleaseSubApis(veData1);

    // Delete
    if (!VoiceEngine::Delete(veData1.ve))
    {
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                            "Delete VoE failed");
        return false;
    }

    veData1.ve = NULL;
#endif

    // Clear instance independent Java objects
    VoiceEngine::SetAndroidObjects(NULL, NULL, NULL);

    return true;
}

/////////////////////////////////////////////
// [Base] Initialize VoiceEngine
//
JNIEXPORT jint JNICALL Java_org_webrtc_voiceengine_test_AndroidTest_Init(
        JNIEnv *,
        jobject,
        jboolean enableTrace,
        jboolean useExtTrans)
{
    VALIDATE_BASE_POINTER;

    if (enableTrace)
    {
        if (0 != VoiceEngine::SetTraceFile("/sdcard/trace.txt"))
        {
            __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                                "Could not enable trace");
        }
        if (0 != VoiceEngine::SetTraceFilter(kTraceAll))
        {
            __android_log_write(ANDROID_LOG_WARN, WEBRTC_LOG_TAG,
                                "Could not set trace filter");
        }
    }

    if (useExtTrans)
    {
        VALIDATE_NETWORK_POINTER;
        veData1.extTrans = new my_transportation(veData1.netw);
    }

    int retVal = 0;
#ifdef INIT_FROM_THREAD
    threadTest.RunTest();
    usleep(200000);
#else
    retVal = veData1.base->Init();
#endif
    return retVal;
}

/////////////////////////////////////////////
// [Base] Terminate VoiceEngine
//
JNIEXPORT jint JNICALL Java_org_webrtc_voiceengine_test_AndroidTest_Terminate(
        JNIEnv *,
        jobject)
{
    VALIDATE_BASE_POINTER;

    jint retVal = veData1.base->Terminate();

    delete veData1.extTrans;
    veData1.extTrans = NULL;

    return retVal;
}

/////////////////////////////////////////////
// [Base] Create channel
//
JNIEXPORT jint JNICALL
Java_org_webrtc_voiceengine_test_AndroidTest_CreateChannel(
        JNIEnv *,
        jobject)
{
    VALIDATE_BASE_POINTER;
    jint channel = veData1.base->CreateChannel();

    if (veData1.extTrans)
    {
        VALIDATE_NETWORK_POINTER;
        __android_log_print(ANDROID_LOG_DEBUG, WEBRTC_LOG_TAG,
                            "Enabling external transport on channel %d",
                            channel);
        if (veData1.netw->RegisterExternalTransport(channel, *veData1.extTrans)
                < 0)
        {
            __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                                "Could not set external transport");
            return -1;
        }
    }

    return channel;
}

/////////////////////////////////////////////
// [Base] Delete channel
//
JNIEXPORT jint JNICALL
Java_org_webrtc_voiceengine_test_AndroidTest_DeleteChannel(
        JNIEnv *,
        jobject,
        jint channel)
{
    VALIDATE_BASE_POINTER;
    return veData1.base->DeleteChannel(channel);
}

/////////////////////////////////////////////
// [Base] SetLocalReceiver
JNIEXPORT jint JNICALL
Java_org_webrtc_voiceengine_test_AndroidTest_SetLocalReceiver(
        JNIEnv *,
        jobject,
        jint channel,
        jint port)
{
    VALIDATE_BASE_POINTER;
    return veData1.base->SetLocalReceiver(channel, port);
}

/////////////////////////////////////////////
// [Base] SetSendDestination
//
JNIEXPORT jint JNICALL
Java_org_webrtc_voiceengine_test_AndroidTest_SetSendDestination(
        JNIEnv *env,
        jobject,
        jint channel,
        jint port,
        jstring ipaddr)
{
    VALIDATE_BASE_POINTER;

    const char* ipaddrNative = env->GetStringUTFChars(ipaddr, NULL);
    if (!ipaddrNative)
    {
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                            "Could not get UTF string");
        return -1;
    }

    jint retVal = veData1.base->SetSendDestination(channel, port, ipaddrNative);

    env->ReleaseStringUTFChars(ipaddr, ipaddrNative);

    return retVal;
}

/////////////////////////////////////////////
// [Base] StartListen
//
JNIEXPORT jint JNICALL Java_org_webrtc_voiceengine_test_AndroidTest_StartListen(
        JNIEnv *,
        jobject,
        jint channel)
{
    VALIDATE_BASE_POINTER;
    int retVal = veData1.base->StartReceive(channel);

    return retVal;
}

/////////////////////////////////////////////
// [Base] Start playout
//
JNIEXPORT jint JNICALL
Java_org_webrtc_voiceengine_test_AndroidTest_StartPlayout(
        JNIEnv *,
        jobject,
        jint channel)
{
    VALIDATE_BASE_POINTER;
    int retVal = veData1.base->StartPlayout(channel);

    return retVal;
}

/////////////////////////////////////////////
// [Base] Start send
//
JNIEXPORT jint JNICALL Java_org_webrtc_voiceengine_test_AndroidTest_StartSend(
        JNIEnv *,
        jobject,
        jint channel)
{
    /*    int dscp(0), serviceType(-1), overrideDscp(0), res(0);
     bool gqosEnabled(false), useSetSockOpt(false);

     if (veData1.netw->SetSendTOS(channel, 13, useSetSockOpt) != 0)
     {
     __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
         "Failed to set TOS");
     return -1;
     }

     res = veData1.netw->GetSendTOS(channel, dscp, useSetSockOpt);
     if (res != 0 || dscp != 13 || useSetSockOpt != true)
     {
     __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
         "Failed to get TOS");
     return -1;
     } */

    /* if (veData1.rtp_rtcp->SetFECStatus(channel, 1) != 0)
     {
     __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
         "Failed to enable FEC");
     return -1;
     } */

    VALIDATE_BASE_POINTER;
    int retVal = veData1.base->StartSend(channel);

    return retVal;
}

/////////////////////////////////////////////
// [Base] Stop listen
//
JNIEXPORT jint JNICALL Java_org_webrtc_voiceengine_test_AndroidTest_StopListen(
        JNIEnv *,
        jobject,
        jint channel)
{
    VALIDATE_BASE_POINTER;
    return veData1.base->StopReceive(channel);
}

/////////////////////////////////////////////
// [Base] Stop playout
//
JNIEXPORT jint JNICALL Java_org_webrtc_voiceengine_test_AndroidTest_StopPlayout(
        JNIEnv *,
        jobject,
        jint channel)
{
    VALIDATE_BASE_POINTER;
    return veData1.base->StopPlayout(channel);
}

/////////////////////////////////////////////
// [Base] Stop send
//
JNIEXPORT jint JNICALL Java_org_webrtc_voiceengine_test_AndroidTest_StopSend(
        JNIEnv *,
        jobject,
        jint channel)
{
    /* if (veData1.rtp_rtcp->SetFECStatus(channel, 0) != 0)
     {
     __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
         "Failed to disable FEC");
     return -1;
     } */

    VALIDATE_BASE_POINTER;
    return veData1.base->StopSend(channel);
}

/////////////////////////////////////////////
// [codec] Number of codecs
//
JNIEXPORT jint JNICALL Java_org_webrtc_voiceengine_test_AndroidTest_NumOfCodecs(
        JNIEnv *,
        jobject)
{
    VALIDATE_CODEC_POINTER;
    return veData1.codec->NumOfCodecs();
}

/////////////////////////////////////////////
// [codec] Set send codec
//
JNIEXPORT jint JNICALL
Java_org_webrtc_voiceengine_test_AndroidTest_SetSendCodec(
        JNIEnv *,
        jobject,
        jint channel,
        jint index)
{
    VALIDATE_CODEC_POINTER;

    CodecInst codec;

    if (veData1.codec->GetCodec(index, codec) != 0)
    {
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                            "Failed to get codec");
        return -1;
    }

    return veData1.codec->SetSendCodec(channel, codec);
}

/////////////////////////////////////////////
// [codec] Set VAD status
//
JNIEXPORT jint JNICALL
Java_org_webrtc_voiceengine_test_AndroidTest_SetVADStatus(
        JNIEnv *,
        jobject,
        jint channel,
        jboolean enable,
        jint mode)
{
    VALIDATE_CODEC_POINTER;

    VadModes VADmode = kVadConventional;

    switch (mode)
    {
        case 0:
            break; // already set
        case 1:
            VADmode = kVadAggressiveLow;
            break;
        case 2:
            VADmode = kVadAggressiveMid;
            break;
        case 3:
            VADmode = kVadAggressiveHigh;
            break;
        default:
            VADmode = (VadModes) 17; // force error
            break;
    }

    return veData1.codec->SetVADStatus(channel, enable, VADmode);
}

/////////////////////////////////////////////
// [apm] SetNSStatus
//
JNIEXPORT jint JNICALL Java_org_webrtc_voiceengine_test_AndroidTest_SetNSStatus(
        JNIEnv *,
        jobject,
        jboolean enable,
        jint mode)
{
    VALIDATE_APM_POINTER;

    NsModes NSmode = kNsDefault;

    switch (mode)
    {
        case 0:
            NSmode = kNsUnchanged;
            break;
        case 1:
            break; // already set
        case 2:
            NSmode = kNsConference;
            break;
        case 3:
            NSmode = kNsLowSuppression;
            break;
        case 4:
            NSmode = kNsModerateSuppression;
            break;
        case 5:
            NSmode = kNsHighSuppression;
            break;
        case 6:
            NSmode = kNsVeryHighSuppression;
            break;
        default:
            NSmode = (NsModes) 17; // force error
            break;
    }

    return veData1.apm->SetNsStatus(enable, NSmode);
}

/////////////////////////////////////////////
// [apm] SetAGCStatus
//
JNIEXPORT jint JNICALL
Java_org_webrtc_voiceengine_test_AndroidTest_SetAGCStatus(
        JNIEnv *,
        jobject,
        jboolean enable,
        jint mode)
{
    VALIDATE_APM_POINTER;

    AgcModes AGCmode = kAgcDefault;

    switch (mode)
    {
        case 0:
            AGCmode = kAgcUnchanged;
            break;
        case 1:
            break; // already set
        case 2:
            AGCmode = kAgcAdaptiveAnalog;
            break;
        case 3:
            AGCmode = kAgcAdaptiveDigital;
            break;
        case 4:
            AGCmode = kAgcFixedDigital;
            break;
        default:
            AGCmode = (AgcModes) 17; // force error
            break;
    }

    /* AgcConfig agcConfig;
     agcConfig.targetLeveldBOv = 3;
     agcConfig.digitalCompressionGaindB = 50;
     agcConfig.limiterEnable = 0;

     if (veData1.apm->SetAGCConfig(agcConfig) != 0)
     {
     __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
         "Failed to set AGC config");
     return -1;
     } */

    return veData1.apm->SetAgcStatus(enable, AGCmode);
}

/////////////////////////////////////////////
// [apm] SetECStatus
//
JNIEXPORT jint JNICALL Java_org_webrtc_voiceengine_test_AndroidTest_SetECStatus(
        JNIEnv *,
        jobject,
        jboolean enable,
        jint mode)
{
    VALIDATE_APM_POINTER;

    EcModes ECmode = kEcDefault;

    switch (mode)
    {
        case 0:
            ECmode = kEcDefault;
            break;
        case 1:
            break; // already set
        case 2:
            ECmode = kEcConference;
            break;
        case 3:
            ECmode = kEcAec;
            break;
        case 4:
            ECmode = kEcAecm;
            break;
        default:
            ECmode = (EcModes) 17; // force error
            break;
    }

    return veData1.apm->SetEcStatus(enable, ECmode);
}

/////////////////////////////////////////////
// [File] Start play file locally
//
JNIEXPORT jint JNICALL
Java_org_webrtc_voiceengine_test_AndroidTest_StartPlayingFileLocally(
        JNIEnv * env,
        jobject,
        jint channel,
        jstring fileName,
        jboolean loop)
{
    VALIDATE_FILE_POINTER;

    const char* fileNameNative = env->GetStringUTFChars(fileName, NULL);
    if (!fileNameNative)
    {
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                            "Could not get UTF string");
        return -1;
    }

    jint retVal = veData1.file->StartPlayingFileLocally(channel,
                                                        fileNameNative, loop);

    env->ReleaseStringUTFChars(fileName, fileNameNative);

    return retVal;
}

/////////////////////////////////////////////
// [File] Stop play file locally
//
JNIEXPORT jint JNICALL
Java_org_webrtc_voiceengine_test_AndroidTest_StopPlayingFileLocally(
        JNIEnv *,
        jobject,
        jint channel)
{
    VALIDATE_FILE_POINTER;
    return veData1.file->StopPlayingFileLocally(channel);
}

/*
 * Class:     org_webrtc_voiceengine_test_AndroidTest
 * Method:    StartRecordingPlayout
 * Signature: (ILjava/lang/String;Z)I
 */
JNIEXPORT jint JNICALL
Java_org_webrtc_voiceengine_test_AndroidTest_StartRecordingPlayout(
        JNIEnv * env,
        jobject,
        jint channel,
        jstring fileName,
        jboolean)
{
    VALIDATE_FILE_POINTER;

    const char* fileNameNative = env->GetStringUTFChars(fileName, NULL);
    if (!fileNameNative)
    {
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                            "Could not get UTF string");
        return -1;
    }

    jint retVal = veData1.file->StartRecordingPlayout(channel, fileNameNative,
                                                      0);

    env->ReleaseStringUTFChars(fileName, fileNameNative);

    return retVal;
}

/////////////////////////////////////////////
// [File] Stop Recording Playout
//
JNIEXPORT jint JNICALL
Java_org_webrtc_voiceengine_test_AndroidTest_StopRecordingPlayout(
        JNIEnv *,
        jobject,
        jint channel)
{
    VALIDATE_FILE_POINTER;
    return veData1.file->StopRecordingPlayout(channel);
}

/////////////////////////////////////////////
// [File] Start playing file as microphone
//
JNIEXPORT jint JNICALL
Java_org_webrtc_voiceengine_test_AndroidTest_StartPlayingFileAsMicrophone(
        JNIEnv *env,
        jobject,
        jint channel,
        jstring fileName,
        jboolean loop)
{
    VALIDATE_FILE_POINTER;

    const char* fileNameNative = env->GetStringUTFChars(fileName, NULL);
    if (!fileNameNative)
    {
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                            "Could not get UTF string");
        return -1;
    }

    jint retVal = veData1.file->StartPlayingFileAsMicrophone(channel,
                                                             fileNameNative,
                                                             loop);

    env->ReleaseStringUTFChars(fileName, fileNameNative);

    return retVal;
}

/////////////////////////////////////////////
// [File] Stop playing file as microphone
//
JNIEXPORT jint JNICALL
Java_org_webrtc_voiceengine_test_AndroidTest_StopPlayingFileAsMicrophone(
        JNIEnv *,
        jobject,
        jint channel)
{
    VALIDATE_FILE_POINTER;
    return veData1.file->StopPlayingFileAsMicrophone(channel);
}

/////////////////////////////////////////////
// [Volume] Set speaker volume
//
JNIEXPORT jint JNICALL
Java_org_webrtc_voiceengine_test_AndroidTest_SetSpeakerVolume(
        JNIEnv *,
        jobject,
        jint level)
{
    VALIDATE_VOLUME_POINTER;
    if (veData1.volume->SetSpeakerVolume(level) != 0)
    {
        return -1;
    }

    unsigned int storedVolume = 0;
    if (veData1.volume->GetSpeakerVolume(storedVolume) != 0)
    {
        return -1;
    }

    if (storedVolume != level)
    {
        return -1;
    }

    return 0;
}

/////////////////////////////////////////////
// [Hardware] Set loudspeaker status
//
JNIEXPORT jint JNICALL
Java_org_webrtc_voiceengine_test_AndroidTest_SetLoudspeakerStatus(
        JNIEnv *,
        jobject,
        jboolean enable)
{
    VALIDATE_HARDWARE_POINTER;
    if (veData1.hardware->SetLoudspeakerStatus(enable) != 0)
    {
        return -1;
    }

    /*VALIDATE_RTP_RTCP_POINTER;

     if (veData1.rtp_rtcp->SetFECStatus(0, enable, -1) != 0)
     {
     __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
         "Could not set FEC");
     return -1;
     }
     else if(enable)
     {
     __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
         "Could enable FEC");
     }
     else
     {
     __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
         "Could disable FEC");
     }*/

    return 0;
}

//////////////////////////////////////////////////////////////////
// "Local" functions (i.e. not Java accessible)
//////////////////////////////////////////////////////////////////

/////////////////////////////////////////////
// Get all sub-APIs
//
bool GetSubApis(VoiceEngineData &veData)
{
    bool getOK = true;

    // Base
    veData.base = VoEBase::GetInterface(veData.ve);
    if (!veData.base)
    {
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                            "Get base sub-API failed");
        getOK = false;
    }

    // Codec
    veData.codec = VoECodec::GetInterface(veData.ve);
    if (!veData.codec)
    {
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                            "Get codec sub-API failed");
        getOK = false;
    }

    // File
    veData.file = VoEFile::GetInterface(veData.ve);
    if (!veData.file)
    {
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                            "Get file sub-API failed");
        getOK = false;
    }

    // Network
    veData.netw = VoENetwork::GetInterface(veData.ve);
    if (!veData.netw)
    {
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                            "Get network sub-API failed");
        getOK = false;
    }

    // AudioProcessing module
    veData.apm = VoEAudioProcessing::GetInterface(veData.ve);
    if (!veData.apm)
    {
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                            "Get apm sub-API failed");
        getOK = false;
    }

    // Volume
    veData.volume = VoEVolumeControl::GetInterface(veData.ve);
    if (!veData.volume)
    {
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                            "Get volume sub-API failed");
        getOK = false;
    }

    // Hardware
    veData.hardware = VoEHardware::GetInterface(veData.ve);
    if (!veData.hardware)
    {
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                            "Get hardware sub-API failed");
        getOK = false;
    }

    // RTP / RTCP
    veData.rtp_rtcp = VoERTP_RTCP::GetInterface(veData.ve);
    if (!veData.rtp_rtcp)
    {
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                            "Get rtp_rtcp sub-API failed");
        getOK = false;
    }

    // Encrypt
    veData.encrypt = VoEEncryption::GetInterface(veData.ve);
    if (!veData.encrypt)
    {
        __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                            "Get encrypt sub-API failed");
        getOK = false;
    }

    return getOK;
}

/////////////////////////////////////////////
// Release all sub-APIs
//
bool ReleaseSubApis(VoiceEngineData &veData)
{
    bool releaseOK = true;

    // Base
    if (veData.base)
    {
        if (0 != veData.base->Release())
        {
            __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                                "Release base sub-API failed");
            releaseOK = false;
        }
        else
        {
            veData.base = NULL;
        }
    }

    // Codec
    if (veData.codec)
    {
        if (0 != veData.codec->Release())
        {
            __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                                "Release codec sub-API failed");
            releaseOK = false;
        }
        else
        {
            veData.codec = NULL;
        }
    }

    // File
    if (veData.file)
    {
        if (0 != veData.file->Release())
        {
            __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                                "Release file sub-API failed");
            releaseOK = false;
        }
        else
        {
            veData.file = NULL;
        }
    }

    // Network
    if (veData.netw)
    {
        if (0 != veData.netw->Release())
        {
            __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                                "Release network sub-API failed");
            releaseOK = false;
        }
        else
        {
            veData.netw = NULL;
        }
    }

    // apm
    if (veData.apm)
    {
        if (0 != veData.apm->Release())
        {
            __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                                "Release apm sub-API failed");
            releaseOK = false;
        }
        else
        {
            veData.apm = NULL;
        }
    }

    // Volume
    if (veData.volume)
    {
        if (0 != veData.volume->Release())
        {
            __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                                "Release volume sub-API failed");
            releaseOK = false;
        }
        else
        {
            veData.volume = NULL;
        }
    }

    // Hardware
    if (veData.hardware)
    {
        if (0 != veData.hardware->Release())
        {
            __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                                "Release hardware sub-API failed");
            releaseOK = false;
        }
        else
        {
            veData.hardware = NULL;
        }
    }

    // RTP RTCP
    if (veData.rtp_rtcp)
    {
        if (0 != veData.rtp_rtcp->Release())
        {
            __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                                "Release rtp_rtcp sub-API failed");
            releaseOK = false;
        }
        else
        {
            veData.rtp_rtcp = NULL;
        }
    }

    // Encrypt
    if (veData.encrypt)
    {
        if (0 != veData.encrypt->Release())
        {
            __android_log_write(ANDROID_LOG_ERROR, WEBRTC_LOG_TAG,
                                "Release encrypt sub-API failed");
            releaseOK = false;
        }
        else
        {
            veData.encrypt = NULL;
        }
    }

    return releaseOK;
}
