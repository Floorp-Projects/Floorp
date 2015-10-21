/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <iostream>
#include <string>
#include <fstream>
#include <unistd.h>
#include <vector>
#include <math.h>

using namespace std;

#include "mozilla/Scoped.h"
#include "mozilla/SyncRunnable.h"
#include <MediaConduitInterface.h>
#include "GmpVideoCodec.h"
#include "nsIEventTarget.h"
#include "FakeMediaStreamsImpl.h"
#include "nsThreadUtils.h"
#include "runnable_utils.h"
#include "signaling/src/common/EncodingConstraints.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

nsCOMPtr<nsIThread> gMainThread;
nsCOMPtr<nsIThread> gGtestThread;
bool gTestsComplete = false;

#include "mtransport_test_utils.h"
MtransportTestUtils *test_utils;

//Video Frame Color
const int COLOR = 0x80; //Gray

//MWC RNG of George Marsaglia
//taken from xiph.org
static int32_t Rz, Rw;
static inline int32_t fast_rand(void)
{
  Rz=36969*(Rz&65535)+(Rz>>16);
  Rw=18000*(Rw&65535)+(Rw>>16);
  return (Rz<<16)+Rw;
}

/**
  * Global structure to store video test results.
  */
struct VideoTestStats
{
 int numRawFramesInserted;
 int numFramesRenderedSuccessfully;
 int numFramesRenderedWrongly;
};

VideoTestStats vidStatsGlobal={0,0,0};

/**
 * A Dummy Video Conduit Tester.
 * The test-case inserts a 640*480 grey imagerevery 33 milliseconds
 * to the video-conduit for encoding and transporting.
 */

class VideoSendAndReceive
{
public:
  VideoSendAndReceive():width(640),
                        height(480),
			rate(30)
  {
  }

  ~VideoSendAndReceive()
  {
  }

  void SetDimensions(int w, int h)
  {
    width = w;
    height = h;
  }
  void SetRate(int r) {
    rate = r;
  }
  void Init(RefPtr<mozilla::VideoSessionConduit> aSession)
  {
        mSession = aSession;
        mLen = ((width * height) * 3 / 2);
        mFrame = (uint8_t*) PR_MALLOC(mLen);
        memset(mFrame, COLOR, mLen);
        numFrames = 121;
  }

  void GenerateAndReadSamples()
  {
    do
    {
      mSession->SendVideoFrame((unsigned char*)mFrame,
                                mLen,
                                width,
                                height,
                                mozilla::kVideoI420,
                                0);
      PR_Sleep(PR_MillisecondsToInterval(1000/rate));
      vidStatsGlobal.numRawFramesInserted++;
      numFrames--;
    } while(numFrames >= 0);
  }

private:
RefPtr<mozilla::VideoSessionConduit> mSession;
mozilla::ScopedDeletePtr<uint8_t> mFrame;
int mLen;
int width, height;
int rate;
int numFrames;
};



/**
 * A Dummy AudioConduit Tester
 * The test reads PCM samples of a standard test file and
 * passws to audio-conduit for encoding, RTPfication and
 * decoding ebery 10 milliseconds.
 * This decoded samples are read-off the conduit for writing
 * into output audio file in PCM format.
 */
class AudioSendAndReceive
{
public:
  static const unsigned int PLAYOUT_SAMPLE_FREQUENCY; //default is 16000
  static const unsigned int PLAYOUT_SAMPLE_LENGTH; //default is 160000

  AudioSendAndReceive()
  {
  }

  ~AudioSendAndReceive()
  {
  }

 void Init(RefPtr<mozilla::AudioSessionConduit> aSession,
           RefPtr<mozilla::AudioSessionConduit> aOtherSession,
           std::string fileIn, std::string fileOut)
  {

    mSession = aSession;
    mOtherSession = aOtherSession;
    iFile = fileIn;
    oFile = fileOut;
 }

  //Kick start the test
  void GenerateAndReadSamples();

private:

  RefPtr<mozilla::AudioSessionConduit> mSession;
  RefPtr<mozilla::AudioSessionConduit> mOtherSession;
  std::string iFile;
  std::string oFile;

  int WriteWaveHeader(int rate, int channels, FILE* outFile);
  int FinishWaveHeader(FILE* outFile);
  void GenerateMusic(int16_t* buf, int len);
};

const unsigned int AudioSendAndReceive::PLAYOUT_SAMPLE_FREQUENCY = 16000;
const unsigned int AudioSendAndReceive::PLAYOUT_SAMPLE_LENGTH  = 160000;

int AudioSendAndReceive::WriteWaveHeader(int rate, int channels, FILE* outFile)
{
  //Hardcoded for 16 bit samples
  unsigned char header[] = {
    // File header
    0x52, 0x49, 0x46, 0x46, // 'RIFF'
    0x00, 0x00, 0x00, 0x00, // chunk size
    0x57, 0x41, 0x56, 0x45, // 'WAVE'
    // fmt chunk. We always write 16-bit samples.
    0x66, 0x6d, 0x74, 0x20, // 'fmt '
    0x10, 0x00, 0x00, 0x00, // chunk size
    0x01, 0x00,             // WAVE_FORMAT_PCM
    0xFF, 0xFF,             // channels
    0xFF, 0xFF, 0xFF, 0xFF, // sample rate
    0x00, 0x00, 0x00, 0x00, // data rate
    0xFF, 0xFF,             // frame size in bytes
    0x10, 0x00,             // bits per sample
    // data chunk
    0x64, 0x61, 0x74, 0x61, // 'data'
    0xFE, 0xFF, 0xFF, 0x7F  // chunk size
  };

#define set_uint16le(buffer, value) \
  (buffer)[0] = (value) & 0xff; \
  (buffer)[1] = (value) >> 8;
#define set_uint32le(buffer, value) \
  set_uint16le( (buffer), (value) & 0xffff ); \
  set_uint16le( (buffer) + 2, (value) >> 16 );

  // set dynamic header fields
  set_uint16le(header + 22, channels);
  set_uint32le(header + 24, rate);
  set_uint16le(header + 32, channels*2);

  size_t written = fwrite(header, 1, sizeof(header), outFile);
  if (written != sizeof(header)) {
    cerr << "Writing WAV header failed" << endl;
    return -1;
  }

  return 0;
}

// Update the WAVE file header with the written length
int AudioSendAndReceive::FinishWaveHeader(FILE* outFile)
{
  // Measure how much data we've written
  long end = ftell(outFile);
  if (end < 16) {
    cerr << "Couldn't get output file length" << endl;
    return (end < 0) ? end : -1;
  }

  // Update the header
  unsigned char size[4];
  int err = fseek(outFile, 40, SEEK_SET);
  if (err < 0) {
    cerr << "Couldn't seek to WAV file header." << endl;
    return err;
  }
  set_uint32le(size, (end - 44) & 0xffffffff);
  size_t written = fwrite(size, 1, sizeof(size), outFile);
  if (written != sizeof(size)) {
    cerr << "Couldn't write data size to WAV header" << endl;
    return -1;
  }

  // Return to the end
  err = fseek(outFile, 0, SEEK_END);
  if (err < 0) {
    cerr << "Couldn't seek to WAV file end." << endl;
    return err;
  }

  return 0;
}

//Code from xiph.org to generate music of predefined length
void AudioSendAndReceive::GenerateMusic(short* buf, int len)
{
  cerr <<" Generating Input Music " << endl;
  int32_t a1,a2,b1,b2;
  int32_t c1,c2,d1,d2;
  int32_t i,j;
  a1=b1=a2=b2=0;
  c1=c2=d1=d2=0;
  j=0;
  /*60ms silence */
  for(i=0;i<2880;i++)
  {
    buf[i*2]=buf[(i*2)+1]=0;
  }
  for(i=2880;i<len-1;i+=2)
  {
    int32_t r;
    int32_t v1,v2;
    v1=v2=(((j*((j>>12)^((j>>10|j>>12)&26&j>>7)))&128)+128)<<15;
    r=fast_rand();v1+=r&65535;v1-=r>>16;
    r=fast_rand();v2+=r&65535;v2-=r>>16;
    b1=v1-a1+((b1*61+32)>>6);a1=v1;
    b2=v2-a2+((b2*61+32)>>6);a2=v2;
    c1=(30*(c1+b1+d1)+32)>>6;d1=b1;
    c2=(30*(c2+b2+d2)+32)>>6;d2=b2;
    v1=(c1+128)>>8;
    v2=(c2+128)>>8;
    buf[i]=v1>32767?32767:(v1<-32768?-32768:v1);
    buf[i+1]=v2>32767?32767:(v2<-32768?-32768:v2);
    if(i%6==0)j++;
  }
  cerr << "Generating Input Music Done " << endl;
}

//Hardcoded for 16 bit samples for now
void AudioSendAndReceive::GenerateAndReadSamples()
{
   mozilla::ScopedDeletePtr<int16_t> audioInput(new int16_t [PLAYOUT_SAMPLE_LENGTH]);
   mozilla::ScopedDeletePtr<int16_t> audioOutput(new int16_t [PLAYOUT_SAMPLE_LENGTH]);
   short* inbuf;
   int sampleLengthDecoded = 0;
   unsigned int SAMPLES = (PLAYOUT_SAMPLE_FREQUENCY * 10); //10 seconds
   int CHANNELS = 1; //mono audio
   int sampleLengthInBytes = sizeof(int16_t) * PLAYOUT_SAMPLE_LENGTH;
   //generated audio buffer
   inbuf = (short *)moz_xmalloc(sizeof(short)*SAMPLES*CHANNELS);
   memset(audioInput.get(),0,sampleLengthInBytes);
   memset(audioOutput.get(),0,sampleLengthInBytes);
   MOZ_ASSERT(SAMPLES <= PLAYOUT_SAMPLE_LENGTH);

   FILE* inFile = fopen( iFile.c_str(), "wb+");
   if(!inFile) {
     cerr << "Input File Creation Failed " << endl;
     free(inbuf);
     return;
   }

   FILE* outFile = fopen( oFile.c_str(), "wb+");
   if(!outFile) {
     cerr << "Output File Creation Failed " << endl;
     free(inbuf);
     fclose(inFile);
     return;
   }

   //Create input file with the music
   WriteWaveHeader(PLAYOUT_SAMPLE_FREQUENCY, 1, inFile);
   GenerateMusic(inbuf, SAMPLES);
   fwrite(inbuf,1,SAMPLES*sizeof(inbuf[0])*CHANNELS,inFile);
   FinishWaveHeader(inFile);
   fclose(inFile);

   WriteWaveHeader(PLAYOUT_SAMPLE_FREQUENCY, 1, outFile);
   unsigned int numSamplesReadFromInput = 0;
   do
   {
    if(!memcpy(audioInput.get(), inbuf, sampleLengthInBytes))
    {
      free(inbuf);
      fclose(outFile);
      return;
    }

    numSamplesReadFromInput += PLAYOUT_SAMPLE_LENGTH;
    inbuf += PLAYOUT_SAMPLE_LENGTH;

    mSession->SendAudioFrame(audioInput.get(),
                             PLAYOUT_SAMPLE_LENGTH,
                             PLAYOUT_SAMPLE_FREQUENCY,10);

    PR_Sleep(PR_MillisecondsToInterval(10));
    mOtherSession->GetAudioFrame(audioOutput.get(), PLAYOUT_SAMPLE_FREQUENCY,
                                 10, sampleLengthDecoded);
    if(sampleLengthDecoded == 0)
    {
      cerr << " Zero length Sample " << endl;
    }

    int wrote_  = fwrite (audioOutput.get(), 1 , sampleLengthInBytes, outFile);
    if(wrote_ != sampleLengthInBytes)
    {
      cerr << "Couldn't Write " << sampleLengthInBytes << "bytes" << endl;
      break;
    }
   }while(numSamplesReadFromInput < SAMPLES);

   FinishWaveHeader(outFile);
   free(inbuf);
   fclose(outFile);
}

/**
 * Dummy Video Target for the conduit
 * This class acts as renderer attached to the video conuit
 * As of today we just verify if the frames rendered are exactly
 * the same as frame inserted at the first place
 */
class DummyVideoTarget: public mozilla::VideoRenderer
{
public:
  DummyVideoTarget()
  {
  }

  virtual ~DummyVideoTarget()
  {
  }


  void RenderVideoFrame(const unsigned char* buffer,
                        size_t buffer_size,
                        uint32_t y_stride,
                        uint32_t cbcr_stride,
                        uint32_t time_stamp,
                        int64_t render_time,
                        const mozilla::ImageHandle& handle) override
  {
    RenderVideoFrame(buffer, buffer_size, time_stamp, render_time, handle);
  }

  void RenderVideoFrame(const unsigned char* buffer,
                        size_t buffer_size,
                        uint32_t time_stamp,
                        int64_t render_time,
                        const mozilla::ImageHandle& handle) override
 {
  //write the frame to the file
  if(VerifyFrame(buffer, buffer_size) == 0)
  {
      vidStatsGlobal.numFramesRenderedSuccessfully++;
  } else
  {
      vidStatsGlobal.numFramesRenderedWrongly++;
  }
 }

 void FrameSizeChange(unsigned int, unsigned int, unsigned int) override
 {
    //do nothing
 }

 //This is hardcoded to check if the contents of frame is COLOR
 // as we set while sending.
 int VerifyFrame(const unsigned char* buffer, unsigned int buffer_size)
 {
    int good = 0;
    for(int i=0; i < (int) buffer_size; i++)
    {
      if(buffer[i] == COLOR)
      {
        ++good;
      }
      else
      {
        --good;
      }
    }
   return 0;
 }

};

/**
 *  Webrtc Audio and Video External Transport Class
 *  The functions in this class will be invoked by the conduit
 *  when it has RTP/RTCP frame to transmit.
 *  For everty RTP/RTCP frame we receive, we pass it back
 *  to the conduit for eventual decoding and rendering.
 */
class WebrtcMediaTransport : public mozilla::TransportInterface
{
public:
  WebrtcMediaTransport():numPkts(0),
                       mAudio(false),
                       mVideo(false)
  {
  }

  ~WebrtcMediaTransport()
  {
  }

  virtual nsresult SendRtpPacket(const void* data, int len)
  {
    ++numPkts;
    if(mAudio)
    {
      mOtherAudioSession->ReceivedRTPPacket(data,len);
    } else
    {
      mOtherVideoSession->ReceivedRTPPacket(data,len);
    }
    return NS_OK;
  }

  virtual nsresult SendRtcpPacket(const void* data, int len)
  {
    if(mAudio)
    {
      mOtherAudioSession->ReceivedRTCPPacket(data,len);
    } else
    {
      mOtherVideoSession->ReceivedRTCPPacket(data,len);
    }
    return NS_OK;
  }

  //Treat this object as Audio Transport
  void SetAudioSession(RefPtr<mozilla::AudioSessionConduit> aSession,
                        RefPtr<mozilla::AudioSessionConduit>
                        aOtherSession)
  {
    mAudioSession = aSession;
    mOtherAudioSession = aOtherSession;
    mAudio = true;
  }

  // Treat this object as Video Transport
  void SetVideoSession(RefPtr<mozilla::VideoSessionConduit> aSession,
                       RefPtr<mozilla::VideoSessionConduit>
                       aOtherSession)
  {
    mVideoSession = aSession;
    mOtherVideoSession = aOtherSession;
    mVideo = true;
  }

private:
  RefPtr<mozilla::AudioSessionConduit> mAudioSession;
  RefPtr<mozilla::VideoSessionConduit> mVideoSession;
  RefPtr<mozilla::VideoSessionConduit> mOtherVideoSession;
  RefPtr<mozilla::AudioSessionConduit> mOtherAudioSession;
  int numPkts;
  bool mAudio, mVideo;
};


namespace {

class TransportConduitTest : public ::testing::Test
{
 public:

  TransportConduitTest()
  {
    //input and output file names
    iAudiofilename = "input.wav";
    oAudiofilename = "recorded.wav";
  }

  ~TransportConduitTest()
  {
    mozilla::SyncRunnable::DispatchToThread(gMainThread,
                                            mozilla::WrapRunnable(
                                                this,
                                                &TransportConduitTest::SelfDestruct));
  }

  void SelfDestruct() {
    mAudioSession = nullptr;
    mAudioSession2 = nullptr;
    mAudioTransport = nullptr;

    mVideoSession = nullptr;
    mVideoSession2 = nullptr;
    mVideoRenderer = nullptr;
    mVideoTransport = nullptr;
  }

  //1. Dump audio samples to dummy external transport
  void TestDummyAudioAndTransport()
  {
    //get pointer to AudioSessionConduit
    int err=0;
    mozilla::SyncRunnable::DispatchToThread(gMainThread,
                                            WrapRunnableNMRet(&mAudioSession,
                                                &mozilla::AudioSessionConduit::Create));
    if( !mAudioSession )
      ASSERT_NE(mAudioSession, (void*)nullptr);

    mozilla::SyncRunnable::DispatchToThread(gMainThread,
                                            WrapRunnableNMRet(&mAudioSession2,
                                                &mozilla::AudioSessionConduit::Create));
    if( !mAudioSession2 )
      ASSERT_NE(mAudioSession2, (void*)nullptr);

    WebrtcMediaTransport* xport = new WebrtcMediaTransport();
    ASSERT_NE(xport, (void*)nullptr);
    xport->SetAudioSession(mAudioSession, mAudioSession2);
    mAudioTransport = xport;

    // attach the transport to audio-conduit
    err = mAudioSession->SetTransmitterTransport(mAudioTransport);
    ASSERT_EQ(mozilla::kMediaConduitNoError, err);
    err = mAudioSession2->SetReceiverTransport(mAudioTransport);
    ASSERT_EQ(mozilla::kMediaConduitNoError, err);

    //configure send and recv codecs on the audio-conduit
    //mozilla::AudioCodecConfig cinst1(124,"PCMU",8000,80,1,64000);
    mozilla::AudioCodecConfig cinst1(124,"opus",48000,960,1,64000);
    mozilla::AudioCodecConfig cinst2(125,"L16",16000,320,1,256000);


    std::vector<mozilla::AudioCodecConfig*> rcvCodecList;
    rcvCodecList.push_back(&cinst1);
    rcvCodecList.push_back(&cinst2);

    err = mAudioSession->ConfigureSendMediaCodec(&cinst1);
    ASSERT_EQ(mozilla::kMediaConduitNoError, err);
    err = mAudioSession->ConfigureRecvMediaCodecs(rcvCodecList);
    ASSERT_EQ(mozilla::kMediaConduitNoError, err);

    err = mAudioSession2->ConfigureSendMediaCodec(&cinst1);
    ASSERT_EQ(mozilla::kMediaConduitNoError, err);
    err = mAudioSession2->ConfigureRecvMediaCodecs(rcvCodecList);
    ASSERT_EQ(mozilla::kMediaConduitNoError, err);

    //start generating samples
    audioTester.Init(mAudioSession,mAudioSession2, iAudiofilename,oAudiofilename);
    cerr << "   ******************************************************** " << endl;
    cerr << "    Generating Audio Samples " << endl;
    cerr << "   ******************************************************** " << endl;
    PR_Sleep(PR_SecondsToInterval(2));
    audioTester.GenerateAndReadSamples();
    PR_Sleep(PR_SecondsToInterval(2));
    cerr << "   ******************************************************** " << endl;
    cerr << "    Input Audio  File                " << iAudiofilename << endl;
    cerr << "    Output Audio File                " << oAudiofilename << endl;
    cerr << "   ******************************************************** " << endl;
  }

  //2. Dump audio samples to dummy external transport
  void TestDummyVideoAndTransport(bool send_vp8 = true, const char *source_file = nullptr)
  {
    int err = 0;
    //get pointer to VideoSessionConduit
    mozilla::SyncRunnable::DispatchToThread(gMainThread,
                                            WrapRunnableNMRet(&mVideoSession,
                                                &mozilla::VideoSessionConduit::Create));
    if( !mVideoSession )
      ASSERT_NE(mVideoSession, (void*)nullptr);

   // This session is for other one
    mozilla::SyncRunnable::DispatchToThread(gMainThread,
                                            WrapRunnableNMRet(&mVideoSession2,
                                                &mozilla::VideoSessionConduit::Create));
    if( !mVideoSession2 )
      ASSERT_NE(mVideoSession2,(void*)nullptr);

    if (!send_vp8) {
      SetGmpCodecs();
    }

    mVideoRenderer = new DummyVideoTarget();
    ASSERT_NE(mVideoRenderer, (void*)nullptr);

    WebrtcMediaTransport* xport = new WebrtcMediaTransport();
    ASSERT_NE(xport, (void*)nullptr);
    xport->SetVideoSession(mVideoSession,mVideoSession2);
    mVideoTransport = xport;

    // attach the transport and renderer to video-conduit
    err = mVideoSession2->AttachRenderer(mVideoRenderer);
    ASSERT_EQ(mozilla::kMediaConduitNoError, err);
    err = mVideoSession->SetTransmitterTransport(mVideoTransport);
    ASSERT_EQ(mozilla::kMediaConduitNoError, err);
    err = mVideoSession2->SetReceiverTransport(mVideoTransport);
    ASSERT_EQ(mozilla::kMediaConduitNoError, err);

    mozilla::EncodingConstraints constraints;
    //configure send and recv codecs on theconduit
    mozilla::VideoCodecConfig cinst1(120, "VP8", constraints);
    mozilla::VideoCodecConfig cinst2(124, "I420", constraints);


    std::vector<mozilla::VideoCodecConfig* > rcvCodecList;
    rcvCodecList.push_back(&cinst1);
    rcvCodecList.push_back(&cinst2);

    err = mVideoSession->ConfigureSendMediaCodec(
        send_vp8 ? &cinst1 : &cinst2);

    ASSERT_EQ(mozilla::kMediaConduitNoError, err);

    err = mVideoSession2->ConfigureSendMediaCodec(
        send_vp8 ? &cinst1 : &cinst2);

    ASSERT_EQ(mozilla::kMediaConduitNoError, err);
    err = mVideoSession2->ConfigureRecvMediaCodecs(rcvCodecList);
    ASSERT_EQ(mozilla::kMediaConduitNoError, err);

    //start generating samples
    cerr << "   *************************************************" << endl;
    cerr << "    Starting the Video Sample Generation " << endl;
    cerr << "   *************************************************" << endl;
    PR_Sleep(PR_SecondsToInterval(2));
    videoTester.Init(mVideoSession);
    videoTester.GenerateAndReadSamples();
    PR_Sleep(PR_SecondsToInterval(2));

    cerr << "   **************************************************" << endl;
    cerr << "    Done With The Testing  " << endl;
    cerr << "    VIDEO TEST STATS  "  << endl;
    cerr << "    Num Raw Frames Inserted: "<<
                                        vidStatsGlobal.numRawFramesInserted << endl;
    cerr << "    Num Frames Successfully Rendered: "<<
                                        vidStatsGlobal.numFramesRenderedSuccessfully << endl;
    cerr << "    Num Frames Wrongly Rendered: "<<
                                        vidStatsGlobal.numFramesRenderedWrongly << endl;

    cerr << "    Done With The Testing  " << endl;

    cerr << "   **************************************************" << endl;
    ASSERT_EQ(0, vidStatsGlobal.numFramesRenderedWrongly);
    if (send_vp8) {
	ASSERT_EQ(vidStatsGlobal.numRawFramesInserted,
		  vidStatsGlobal.numFramesRenderedSuccessfully);
    }
    else {
	// Allow some fudge because there seems to be some buffering.
	// TODO(ekr@rtfm.com): Fix this.
	ASSERT_GE(vidStatsGlobal.numRawFramesInserted,
		  vidStatsGlobal.numFramesRenderedSuccessfully);
	ASSERT_LE(vidStatsGlobal.numRawFramesInserted,
		  vidStatsGlobal.numFramesRenderedSuccessfully + 2);
    }
  }

 void TestVideoConduitCodecAPI()
  {
    int err = 0;
    RefPtr<mozilla::VideoSessionConduit> videoSession;
    //get pointer to VideoSessionConduit
    mozilla::SyncRunnable::DispatchToThread(gMainThread,
                                            WrapRunnableNMRet(&videoSession,
                                                &mozilla::VideoSessionConduit::Create));
    if( !videoSession )
      ASSERT_NE(videoSession, (void*)nullptr);

    //Test Configure Recv Codec APIS
    cerr << "   *************************************************" << endl;
    cerr << "    Test Receive Codec Configuration API Now " << endl;
    cerr << "   *************************************************" << endl;

    std::vector<mozilla::VideoCodecConfig* > rcvCodecList;

    //Same APIs
    cerr << "   *************************************************" << endl;
    cerr << "    1. Same Codec (VP8) Repeated Twice " << endl;
    cerr << "   *************************************************" << endl;

    mozilla::EncodingConstraints constraints;
    mozilla::VideoCodecConfig cinst1(120, "VP8", constraints);
    mozilla::VideoCodecConfig cinst2(120, "VP8", constraints);
    rcvCodecList.push_back(&cinst1);
    rcvCodecList.push_back(&cinst2);
    err = videoSession->ConfigureRecvMediaCodecs(rcvCodecList);
    EXPECT_NE(err,mozilla::kMediaConduitNoError);
    rcvCodecList.pop_back();
    rcvCodecList.pop_back();


    PR_Sleep(PR_SecondsToInterval(2));
    cerr << "   *************************************************" << endl;
    cerr << "    2. Codec With Invalid Payload Names " << endl;
    cerr << "   *************************************************" << endl;
    cerr << "   Setting payload 1 with name: I4201234tttttthhhyyyy89087987y76t567r7756765rr6u6676" << endl;
    cerr << "   Setting payload 2 with name of zero length" << endl;

    mozilla::VideoCodecConfig cinst3(124, "I4201234tttttthhhyyyy89087987y76t567r7756765rr6u6676", constraints);
    mozilla::VideoCodecConfig cinst4(124, "", constraints);

    rcvCodecList.push_back(&cinst3);
    rcvCodecList.push_back(&cinst4);

    err = videoSession->ConfigureRecvMediaCodecs(rcvCodecList);
    EXPECT_TRUE(err != mozilla::kMediaConduitNoError);
    rcvCodecList.pop_back();
    rcvCodecList.pop_back();


    PR_Sleep(PR_SecondsToInterval(2));
    cerr << "   *************************************************" << endl;
    cerr << "    3. Null Codec Parameter  " << endl;
    cerr << "   *************************************************" << endl;

    rcvCodecList.push_back(0);

    err = videoSession->ConfigureRecvMediaCodecs(rcvCodecList);
    EXPECT_TRUE(err != mozilla::kMediaConduitNoError);
    rcvCodecList.pop_back();

    cerr << "   *************************************************" << endl;
    cerr << "    Test Send Codec Configuration API Now " << endl;
    cerr << "   *************************************************" << endl;

    cerr << "   *************************************************" << endl;
    cerr << "    1. Same Codec (VP8) Repeated Twice " << endl;
    cerr << "   *************************************************" << endl;


    err = videoSession->ConfigureSendMediaCodec(&cinst1);
    EXPECT_EQ(mozilla::kMediaConduitNoError, err);
    err = videoSession->ConfigureSendMediaCodec(&cinst1);
    EXPECT_EQ(mozilla::kMediaConduitCodecInUse, err);


    cerr << "   *************************************************" << endl;
    cerr << "    2. Codec With Invalid Payload Names " << endl;
    cerr << "   *************************************************" << endl;
    cerr << "   Setting payload with name: I4201234tttttthhhyyyy89087987y76t567r7756765rr6u6676" << endl;

    err = videoSession->ConfigureSendMediaCodec(&cinst3);
    EXPECT_TRUE(err != mozilla::kMediaConduitNoError);

    cerr << "   *************************************************" << endl;
    cerr << "    3. Null Codec Parameter  " << endl;
    cerr << "   *************************************************" << endl;

    err = videoSession->ConfigureSendMediaCodec(nullptr);
    EXPECT_TRUE(err != mozilla::kMediaConduitNoError);

    mozilla::SyncRunnable::DispatchToThread(gMainThread,
                                            WrapRunnable(
                                                videoSession.forget().take(),
                                                &mozilla::VideoSessionConduit::Release));
  }

  void DumpMaxFs(int orig_width, int orig_height, int max_fs,
                 int new_width, int new_height)
  {
    cerr << "Applying max_fs=" << max_fs << " to input resolution " <<
                 orig_width << "x" << orig_height << endl;
    cerr << "New resolution: " << new_width << "x" << new_height << endl;
    cerr << endl;
  }

  // Calculate new resolution for sending video by applying max-fs constraint.
  void GetVideoResolutionWithMaxFs(int orig_width, int orig_height, int max_fs,
                                   int *new_width, int *new_height)
  {
    int err = 0;

    // Get pointer to VideoSessionConduit.
    mozilla::SyncRunnable::DispatchToThread(gMainThread,
                                            WrapRunnableNMRet(&mVideoSession,
                                                &mozilla::VideoSessionConduit::Create));
    if( !mVideoSession )
      ASSERT_NE(mVideoSession, (void*)nullptr);

    mozilla::EncodingConstraints constraints;
    constraints.maxFs = max_fs;
    // Configure send codecs on the conduit.
    mozilla::VideoCodecConfig cinst1(120, "VP8", constraints);

    err = mVideoSession->ConfigureSendMediaCodec(&cinst1);
    ASSERT_EQ(mozilla::kMediaConduitNoError, err);

    // Send one frame.
    MOZ_ASSERT(!(orig_width & 1));
    MOZ_ASSERT(!(orig_height & 1));
    int len = ((orig_width * orig_height) * 3 / 2);
    uint8_t* frame = (uint8_t*) PR_MALLOC(len);

    memset(frame, COLOR, len);
    mVideoSession->SendVideoFrame((unsigned char*)frame,
                                  len,
                                  orig_width,
                                  orig_height,
                                  mozilla::kVideoI420,
                                  0);
    PR_Free(frame);

    // Get the new resolution as adjusted by the max-fs constraint.
    *new_width = mVideoSession->SendingWidth();
    *new_height = mVideoSession->SendingHeight();
  }

  void TestVideoConduitMaxFs()
  {
    int orig_width, orig_height, width, height, max_fs;

    // No limitation.
    cerr << "Test no max-fs limition" << endl;
    orig_width = 640;
    orig_height = 480;
    max_fs = 0;
    GetVideoResolutionWithMaxFs(orig_width, orig_height, max_fs, &width, &height);
    DumpMaxFs(orig_width, orig_height, max_fs, width, height);
    ASSERT_EQ(width, 640);
    ASSERT_EQ(height, 480);

    // VGA to QVGA.
    cerr << "Test resizing from VGA to QVGA" << endl;
    orig_width = 640;
    orig_height = 480;
    max_fs = 300;
    GetVideoResolutionWithMaxFs(orig_width, orig_height, max_fs, &width, &height);
    DumpMaxFs(orig_width, orig_height, max_fs, width, height);
    ASSERT_EQ(width, 320);
    ASSERT_EQ(height, 240);

    // Extreme input resolution.
    cerr << "Test extreme input resolution" << endl;
    orig_width = 3072;
    orig_height = 100;
    max_fs = 300;
    GetVideoResolutionWithMaxFs(orig_width, orig_height, max_fs, &width, &height);
    DumpMaxFs(orig_width, orig_height, max_fs, width, height);
    ASSERT_EQ(width, 768);
    ASSERT_EQ(height, 26);

    // Small max-fs.
    cerr << "Test small max-fs (case 1)" << endl;
    orig_width = 8;
    orig_height = 32;
    max_fs = 1;
    GetVideoResolutionWithMaxFs(orig_width, orig_height, max_fs, &width, &height);
    DumpMaxFs(orig_width, orig_height, max_fs, width, height);
    ASSERT_EQ(width, 4);
    ASSERT_EQ(height, 16);

    // Small max-fs.
    cerr << "Test small max-fs (case 2)" << endl;
    orig_width = 4;
    orig_height = 50;
    max_fs = 1;
    GetVideoResolutionWithMaxFs(orig_width, orig_height, max_fs, &width, &height);
    DumpMaxFs(orig_width, orig_height, max_fs, width, height);
    ASSERT_EQ(width, 2);
    ASSERT_EQ(height, 16);

    // Small max-fs.
    cerr << "Test small max-fs (case 3)" << endl;
    orig_width = 872;
    orig_height = 136;
    max_fs = 3;
    GetVideoResolutionWithMaxFs(orig_width, orig_height, max_fs, &width, &height);
    DumpMaxFs(orig_width, orig_height, max_fs, width, height);
    ASSERT_EQ(width, 48);
    ASSERT_EQ(height, 8);

    // Small max-fs.
    cerr << "Test small max-fs (case 4)" << endl;
    orig_width = 160;
    orig_height = 8;
    max_fs = 5;
    GetVideoResolutionWithMaxFs(orig_width, orig_height, max_fs, &width, &height);
    DumpMaxFs(orig_width, orig_height, max_fs, width, height);
    ASSERT_EQ(width, 80);
    ASSERT_EQ(height, 4);

     // Extremely small width and height(see bug 919979).
    cerr << "Test with extremely small width and height" << endl;
    orig_width = 2;
    orig_height = 2;
    max_fs = 5;
    GetVideoResolutionWithMaxFs(orig_width, orig_height, max_fs, &width, &height);
    DumpMaxFs(orig_width, orig_height, max_fs, width, height);
    ASSERT_EQ(width, 2);
    ASSERT_EQ(height, 2);

    // Random values.
    cerr << "Test with random values" << endl;
    for (int i = 0; i < 30; i++) {
      cerr << ".";
      max_fs = rand() % 1000;
      orig_width = ((rand() % 2000) & ~1) + 2;
      orig_height = ((rand() % 2000) & ~1) + 2;

      GetVideoResolutionWithMaxFs(orig_width, orig_height, max_fs,
                                  &width, &height);
      if (max_fs > 0 &&
          ceil(width / 16.) * ceil(height / 16.) > max_fs) {
        DumpMaxFs(orig_width, orig_height, max_fs, width, height);
        ADD_FAILURE();
      }
      if ((width & 1) || (height & 1)) {
        DumpMaxFs(orig_width, orig_height, max_fs, width, height);
        ADD_FAILURE();
      }
    }
    cerr << endl;
 }

  void SetGmpCodecs() {
    mExternalEncoder = mozilla::GmpVideoCodec::CreateEncoder();
    mExternalDecoder = mozilla::GmpVideoCodec::CreateDecoder();
    mozilla::EncodingConstraints constraints;
    mozilla::VideoCodecConfig config(124, "H264", constraints);
    mVideoSession->SetExternalSendCodec(&config, mExternalEncoder);
    mVideoSession2->SetExternalRecvCodec(&config, mExternalDecoder);
  }

 private:
  //Audio Conduit Test Objects
  RefPtr<mozilla::AudioSessionConduit> mAudioSession;
  RefPtr<mozilla::AudioSessionConduit> mAudioSession2;
  RefPtr<mozilla::TransportInterface> mAudioTransport;
  AudioSendAndReceive audioTester;

  //Video Conduit Test Objects
  RefPtr<mozilla::VideoSessionConduit> mVideoSession;
  RefPtr<mozilla::VideoSessionConduit> mVideoSession2;
  RefPtr<mozilla::VideoRenderer> mVideoRenderer;
  RefPtr<mozilla::TransportInterface> mVideoTransport;
  VideoSendAndReceive videoTester;

  mozilla::VideoEncoder* mExternalEncoder;
  mozilla::VideoDecoder* mExternalDecoder;

  std::string fileToPlay;
  std::string fileToRecord;
  std::string iAudiofilename;
  std::string oAudiofilename;
};


// Test 1: Test Dummy External Xport
TEST_F(TransportConduitTest, TestDummyAudioWithTransport) {
  TestDummyAudioAndTransport();
}

// Test 2: Test Dummy External Xport
TEST_F(TransportConduitTest, TestDummyVideoWithTransport) {
  TestDummyVideoAndTransport();
 }

TEST_F(TransportConduitTest, TestVideoConduitExternalCodec) {
  TestDummyVideoAndTransport(false);
}

TEST_F(TransportConduitTest, TestVideoConduitCodecAPI) {
  TestVideoConduitCodecAPI();
 }

TEST_F(TransportConduitTest, TestVideoConduitMaxFs) {
  TestVideoConduitMaxFs();
 }

}  // end namespace

static int test_result;
bool test_finished = false;



// This exists to send as an event to trigger shutdown.
static void tests_complete() {
  gTestsComplete = true;
}

// The GTest thread runs this instead of the main thread so it can
// do things like ASSERT_TRUE_WAIT which you could not do on the main thread.
static int gtest_main(int argc, char **argv) {
  MOZ_ASSERT(!NS_IsMainThread());

  ::testing::InitGoogleTest(&argc, argv);

  int result = RUN_ALL_TESTS();

  // Set the global shutdown flag and tickle the main thread
  // The main thread did not go through Init() so calling Shutdown()
  // on it will not work.
  gMainThread->Dispatch(mozilla::WrapRunnableNM(tests_complete), NS_DISPATCH_SYNC);

  return result;
}

int main(int argc, char **argv)
{
  // This test can cause intermittent oranges on the builders
  CHECK_ENVIRONMENT_FLAG("MOZ_WEBRTC_MEDIACONDUIT_TESTS")

  test_utils = new MtransportTestUtils();

  // Set the main thread global which is this thread.
  nsIThread *thread;
  NS_GetMainThread(&thread);
  gMainThread = thread;

  // Now create the GTest thread and run all of the tests on it
  // When it is complete it will set gTestsComplete
  NS_NewNamedThread("gtest_thread", &thread);
  gGtestThread = thread;

  int result;
  gGtestThread->Dispatch(
    mozilla::WrapRunnableNMRet(&result, gtest_main, argc, argv), NS_DISPATCH_NORMAL);

  // Here we handle the event queue for dispatches to the main thread
  // When the GTest thread is complete it will send one more dispatch
  // with gTestsComplete == true.
  while (!gTestsComplete && NS_ProcessNextEvent());

  gGtestThread->Shutdown();

  delete test_utils;
  return test_result;
}



