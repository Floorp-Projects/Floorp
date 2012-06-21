/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// testAPI.cpp : Defines the entry point for the console application.
//
// NOTES:
//          1. MediaFile library and testAPI.cpp must be built in DEBUG mode for testing.
//

#include <iostream>
#include <stdio.h>
#include <assert.h>

#ifdef WIN32
    #include <windows.h>
    #include <tchar.h>
#endif

#include "common_types.h"
#include "trace.h"

#include "Engineconfigurations.h"
#include "media_file.h"
#include "file_player.h"
#include "file_recorder.h"


bool notify = false, playing = false, recording = false;

// callback class for FileModule
class MyFileModuleCallback : public FileCallback
{
public:
    virtual void PlayNotification( const WebRtc_Word32 id,
                                   const WebRtc_UWord32 durationMs )
    {
        printf("\tReceived PlayNotification from module %ld, durationMs = %ld\n",
               id, durationMs);
        notify = true;
    };

    virtual void RecordNotification( const WebRtc_Word32 id,
                                     const WebRtc_UWord32 durationMs )
    {
        printf("\tReceived RecordNotification from module %ld, durationMs = %ld\n",
               id, durationMs);
        notify = true;
    };

    virtual void PlayFileEnded(const WebRtc_Word32 id)
    {
        printf("\tReceived PlayFileEnded notification from module %ld.\n", id);
        playing = false;
    };

    virtual void RecordFileEnded(const WebRtc_Word32 id)
    {
        printf("\tReceived RecordFileEnded notification from module %ld.\n", id);
        recording = false;
    }
};

// main test app
#ifdef WIN32
int _tmain(int argc, _TCHAR* argv[])
#else
int main(int /*argc*/, char** /*argv*/)
#endif
{
    Trace::CreateTrace();
    Trace::SetTraceFile("testTrace.txt");
    Trace::SetEncryptedTraceFile("testTraceDebug.txt");

    int playId = 1;
    int recordId = 2;

    printf("Welcome to test of FilePlayer and FileRecorder\n");


    ///////////////////////////////////////////////
    //
    // avi test case 1
    //
    ///////////////////////////////////////////////


    // todo PW we need more AVI tests Mp4

    {
        FilePlayer& filePlayer(*FilePlayer::CreateFilePlayer(1, webrtc::kFileFormatAviFile));
        FileRecorder& fileRecorder(*FileRecorder::CreateFileRecorder(1, webrtc::kFileFormatAviFile));

        const char* KFileName = "./tmpAviFileTestCase1_audioI420CIF30fps.avi";

        printf("\tReading from an avi file and writing the information to another \n");
        printf("\tin the same format (I420 CIF 30fps) \n");
        printf("\t\t check file named %s\n", KFileName);

        assert(filePlayer.StartPlayingVideoFile(
           "../../../MediaFile/main/test/files/aviTestCase1_audioI420CIF30fps.avi",
           false, false) == 0);

        // init codecs
         webrtc::VideoCodec videoCodec;
        webrtc::VideoCodec recVideoCodec;
        webrtc::CodecInst audioCodec;
        assert(filePlayer.VideoCodec( videoCodec ) == 0);
        assert(filePlayer.AudioCodec( audioCodec) == 0);

        recVideoCodec = videoCodec;

        assert( fileRecorder.StartRecordingVideoFile(KFileName,
                                                     audioCodec,
                                                     recVideoCodec) == 0);

        assert(fileRecorder.IsRecording());

        WebRtc_UWord32 videoReadSize = static_cast<WebRtc_UWord32>( (videoCodec.width * videoCodec.height * 3.0) / 2.0);

        webrtc::VideoFrame videoFrame;
        videoFrame.VerifyAndAllocate(videoReadSize);

        int  frameCount   = 0;
        bool audioNotDone = true;
        bool videoNotDone =    true;
        AudioFrame audioFrame;

        while( audioNotDone || videoNotDone)
        {
            if(filePlayer.TimeUntilNextVideoFrame() <= 0)
            {
                if(filePlayer.GetVideoFromFile( videoFrame) != 0)
                {
                    // no more video frames
                    break;
                }
                frameCount++;
                videoNotDone = ( videoFrame.Length() > 0);
                videoFrame.SetRenderTime(TickTime::MillisecondTimestamp());
                if( videoNotDone)
                {
                    assert(fileRecorder.RecordVideoToFile(videoFrame) == 0);
                    ::Sleep(10);
                }
            }
             WebRtc_UWord32 decodedDataLengthInSamples;
            if( 0 !=  filePlayer.Get10msAudioFromFile( audioFrame._payloadData, decodedDataLengthInSamples, audioCodec.plfreq))
            {
                audioNotDone = false;
            } else
            {
                audioFrame._frequencyInHz = filePlayer.Frequency();
                audioFrame._payloadDataLengthInSamples = (WebRtc_UWord16)decodedDataLengthInSamples;
                fileRecorder.RecordAudioToFile(audioFrame, &TickTime::Now());
            }
       }
        ::Sleep(100);
        assert(fileRecorder.StopRecording() == 0);
        assert( !fileRecorder.IsRecording());
        assert(frameCount == 135);
        printf("\tGenerated %s\n\n", KFileName);
    }
    ///////////////////////////////////////////////
    //
    // avi test case 2
    //
    ///////////////////////////////////////////////
    {
        FilePlayer& filePlayer(*FilePlayer::CreateFilePlayer(2, webrtc::kFileFormatAviFile));
        FileRecorder& fileRecorder(*FileRecorder::CreateFileRecorder(2, webrtc::kFileFormatAviFile));

        const char* KFileName = "./tmpAviFileTestCase2_audioI420CIF20fps.avi";

        printf("\tWriting information to a avi file and check the written file by \n");
        printf("\treopening it and control codec information.\n");
        printf("\t\t check file named %s all frames should be light green.\n", KFileName);
        // init codecs
        webrtc::VideoCodec videoCodec;
        webrtc::CodecInst      audioCodec;

        memset(&videoCodec, 0, sizeof(videoCodec));

        const char* KVideoCodecName = "I420";
        strcpy(videoCodec.plName, KVideoCodecName);
        videoCodec.plType    = 124;
        videoCodec.maxFramerate = 20;
        videoCodec.height    = 288;
        videoCodec.width     = 352;

        const char* KAudioCodecName = "PCMU";
        strcpy(audioCodec.plname, KAudioCodecName);
        audioCodec.pltype   = 0;
        audioCodec.plfreq   = 8000;
        audioCodec.pacsize  = 80;
        audioCodec.channels = 1;
        audioCodec.rate     = 64000;

        assert( fileRecorder.StartRecordingVideoFile(
            KFileName,
            audioCodec,
            videoCodec) == 0);

        assert(fileRecorder.IsRecording());

        const WebRtc_UWord32 KVideoWriteSize = static_cast< WebRtc_UWord32>( (videoCodec.width * videoCodec.height * 3) / 2);
        webrtc::VideoFrame videoFrame;

        // 10 ms
        AudioFrame audioFrame;
        audioFrame._payloadDataLengthInSamples = audioCodec.plfreq/100;
        memset(audioFrame._payloadData, 0, 2*audioFrame._payloadDataLengthInSamples);
        audioFrame._frequencyInHz = 8000;

        // prepare the video frame
        videoFrame.VerifyAndAllocate(KVideoWriteSize);
        memset(videoFrame.Buffer(), 127, videoCodec.width * videoCodec.height);
        memset(videoFrame.Buffer() +(videoCodec.width * videoCodec.height), 0, videoCodec.width * videoCodec.height/2);
        videoFrame.SetLength(KVideoWriteSize);
        videoFrame.SetHeight(videoCodec.height);
        videoFrame.SetWidth(videoCodec.width);

        // write avi file, with 20 video frames
        const int KWriteNumFrames = 20;
        int       writeFrameCount = 0;
        while(writeFrameCount < KWriteNumFrames)
        {
            // add a video frame
            assert(fileRecorder.RecordVideoToFile(videoFrame) == 0);

            // add 50 ms of audio
            for(int i=0; i<5; i++)
            {
                assert( fileRecorder.RecordAudioToFile(audioFrame) == 0);
            }// for i
            writeFrameCount++;
        }
        ::Sleep(10); // enough tim eto write the queued data to the file
        assert(writeFrameCount == 20);
        assert(fileRecorder.StopRecording() == 0);
        assert( ! fileRecorder.IsRecording());

        assert(filePlayer.StartPlayingVideoFile(KFileName,false, false) == 0);
        assert(filePlayer.IsPlayingFile( ));

        // compare codecs read from file to the ones used when writing the file
        webrtc::VideoCodec readVideoCodec;
        assert(filePlayer.VideoCodec( readVideoCodec ) == 0);
        assert(strcmp(readVideoCodec.plName, videoCodec.plName) == 0);
        assert(readVideoCodec.width      == videoCodec.width);
        assert(readVideoCodec.height     == videoCodec.height);
        assert(readVideoCodec.maxFramerate  == videoCodec.maxFramerate);

        webrtc::CodecInst readAudioCodec;
        assert(filePlayer.AudioCodec( readAudioCodec) == 0);
        assert(strcmp(readAudioCodec.plname, audioCodec.plname) == 0);
        assert(readAudioCodec.pltype     == audioCodec.pltype);
        assert(readAudioCodec.plfreq     == audioCodec.plfreq);
        assert(readAudioCodec.pacsize    == audioCodec.pacsize);
        assert(readAudioCodec.channels   == audioCodec.channels);
        assert(readAudioCodec.rate       == audioCodec.rate);

        assert(filePlayer.StopPlayingFile() == 0);
        assert( ! filePlayer.IsPlayingFile());
        printf("\tGenerated %s\n\n", KFileName);
    }
    ///////////////////////////////////////////////
    //
    // avi test case 3
    //
    ///////////////////////////////////////////////

    {
        FilePlayer& filePlayer(*FilePlayer::CreateFilePlayer(2, webrtc::kFileFormatAviFile));
        FileRecorder& fileRecorder(*FileRecorder::CreateFileRecorder(3, webrtc::kFileFormatAviFile));

        printf("\tReading from an avi file and writing the information to another \n");
        printf("\tin a different format (H.263 CIF 30fps) \n");
        printf("\t\t check file named tmpAviFileTestCase1_audioH263CIF30fps.avi\n");

        assert(filePlayer.StartPlayingVideoFile(
           "../../../MediaFile/main/test/files/aviTestCase1_audioI420CIF30fps.avi",
           false,
           false) == 0);

        // init codecs
         webrtc::VideoCodec videoCodec;
        webrtc::VideoCodec recVideoCodec;
        webrtc::CodecInst      audioCodec;
        assert(filePlayer.VideoCodec( videoCodec ) == 0);
        assert(filePlayer.AudioCodec( audioCodec) == 0);
        recVideoCodec = videoCodec;

        memcpy(recVideoCodec.plName, "H263",5);
        recVideoCodec.startBitrate = 1000;
        recVideoCodec.codecSpecific.H263.quality = 1;
        recVideoCodec.plType = 34;
        recVideoCodec.codecType = webrtc::kVideoCodecH263;

        assert( fileRecorder.StartRecordingVideoFile(
            "./tmpAviFileTestCase1_audioH263CIF30fps.avi",
            audioCodec,
            recVideoCodec) == 0);

        assert(fileRecorder.IsRecording());

        WebRtc_UWord32 videoReadSize = static_cast<WebRtc_UWord32>( (videoCodec.width * videoCodec.height * 3.0) / 2.0);

        webrtc::VideoFrame videoFrame;
        videoFrame.VerifyAndAllocate(videoReadSize);

        int  videoFrameCount   = 0;
        int  audioFrameCount   = 0;
        bool audioNotDone = true;
        bool videoNotDone =    true;
        AudioFrame audioFrame;

        while( audioNotDone || videoNotDone)
        {
            if(filePlayer.TimeUntilNextVideoFrame() <= 0)
            {
                if(filePlayer.GetVideoFromFile( videoFrame) != 0)
                {
                    break;
                }
                videoFrameCount++;
                videoNotDone = ( videoFrame.Length() > 0);
                if( videoNotDone)
                {
                    assert(fileRecorder.RecordVideoToFile(videoFrame) == 0);
                }
            }

            WebRtc_UWord32 decodedDataLengthInSamples;
            if( 0 != filePlayer.Get10msAudioFromFile( audioFrame._payloadData, decodedDataLengthInSamples, audioCodec.plfreq))
            {
                audioNotDone = false;

            } else
            {
                ::Sleep(5);
                audioFrame._frequencyInHz = filePlayer.Frequency();
                audioFrame._payloadDataLengthInSamples = (WebRtc_UWord16)decodedDataLengthInSamples;
                assert(0 == fileRecorder.RecordAudioToFile(audioFrame));

                audioFrameCount++;
            }
        }
        assert(videoFrameCount == 135);
        assert(audioFrameCount == 446); // we will start & stop with a video frame

        assert(fileRecorder.StopRecording() == 0);
        assert( !fileRecorder.IsRecording());
        printf("\tGenerated ./tmpAviFileTestCase1_audioH263CIF30fps.avi\n\n");
    }


    printf("\nTEST completed.\n");

    Trace::ReturnTrace();
    return 0;
}
