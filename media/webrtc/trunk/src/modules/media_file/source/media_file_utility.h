/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Note: the class cannot be used for reading and writing at the same time.
#ifndef WEBRTC_MODULES_MEDIA_FILE_SOURCE_MEDIA_FILE_UTILITY_H_
#define WEBRTC_MODULES_MEDIA_FILE_SOURCE_MEDIA_FILE_UTILITY_H_

#include <stdio.h>

#include "common_types.h"
#include "media_file_defines.h"

namespace webrtc {
class AviFile;
class InStream;
class OutStream;

class ModuleFileUtility
{
public:

    ModuleFileUtility(const WebRtc_Word32 id);
    ~ModuleFileUtility();

#ifdef WEBRTC_MODULE_UTILITY_VIDEO
    // Open the file specified by fileName for reading (relative path is
    // allowed). If loop is true the file will be played until StopPlaying() is
    // called. When end of file is reached the file is read from the start.
    // Only video will be read if videoOnly is true.
    WebRtc_Word32 InitAviReading(const char* fileName, bool videoOnly,
                                 bool loop);

    // Put 10-60ms of audio data from file into the outBuffer depending on
    // codec frame size. bufferLengthInBytes indicates the size of outBuffer.
    // The return value is the number of bytes written to audioBuffer.
    // Note: This API only play mono audio but can be used on file containing
    // audio with more channels (in which case the audio will be coverted to
    // mono).
    WebRtc_Word32 ReadAviAudioData(WebRtc_Word8* outBuffer,
                                   const WebRtc_UWord32 bufferLengthInBytes);

    // Put one video frame into outBuffer. bufferLengthInBytes indicates the
    // size of outBuffer.
    // The return value is the number of bytes written to videoBuffer.
    WebRtc_Word32 ReadAviVideoData(WebRtc_Word8* videoBuffer,
                                   const WebRtc_UWord32 bufferLengthInBytes);

    // Open/create the file specified by fileName for writing audio/video data
    // (relative path is allowed). codecInst specifies the encoding of the audio
    // data. videoCodecInst specifies the encoding of the video data. Only video
    // data will be recorded if videoOnly is true.
    WebRtc_Word32 InitAviWriting(const char* filename,
                                 const CodecInst& codecInst,
                                 const VideoCodec& videoCodecInst,
                                 const bool videoOnly);

    // Write one audio frame, i.e. the bufferLengthinBytes first bytes of
    // audioBuffer, to file. The audio frame size is determined by the
    // codecInst.pacsize parameter of the last sucessfull
    // InitAviWriting(..) call.
    // Note: bufferLength must be exactly one frame.
    WebRtc_Word32 WriteAviAudioData(const WebRtc_Word8* audioBuffer,
                                    WebRtc_UWord32 bufferLengthInBytes);


    // Write one video frame, i.e. the bufferLength first bytes of videoBuffer,
    // to file.
    // Note: videoBuffer can contain encoded data. The codec used must be the
    // same as what was specified by videoCodecInst for the last successfull
    // InitAviWriting(..) call. The videoBuffer must contain exactly
    // one video frame.
    WebRtc_Word32 WriteAviVideoData(const WebRtc_Word8* videoBuffer,
                                    WebRtc_UWord32 bufferLengthInBytes);

    // Stop recording to file or stream.
    WebRtc_Word32 CloseAviFile();

    WebRtc_Word32 VideoCodecInst(VideoCodec& codecInst);
#endif // #ifdef WEBRTC_MODULE_UTILITY_VIDEO

    // Prepare for playing audio from stream.
    // startPointMs and stopPointMs, unless zero, specify what part of the file
    // should be read. From startPointMs ms to stopPointMs ms.
    WebRtc_Word32 InitWavReading(InStream& stream,
                                 const WebRtc_UWord32 startPointMs = 0,
                                 const WebRtc_UWord32 stopPointMs = 0);

    // Put 10-60ms of audio data from stream into the audioBuffer depending on
    // codec frame size. dataLengthInBytes indicates the size of audioBuffer.
    // The return value is the number of bytes written to audioBuffer.
    // Note: This API only play mono audio but can be used on file containing
    // audio with more channels (in which case the audio will be converted to
    // mono).
    WebRtc_Word32 ReadWavDataAsMono(InStream& stream, WebRtc_Word8* audioBuffer,
                                    const WebRtc_UWord32 dataLengthInBytes);

    // Put 10-60ms, depending on codec frame size, of audio data from file into
    // audioBufferLeft and audioBufferRight. The buffers contain the left and
    // right channel of played out stereo audio.
    // dataLengthInBytes  indicates the size of both audioBufferLeft and
    // audioBufferRight.
    // The return value is the number of bytes read for each buffer.
    // Note: This API can only be successfully called for WAV files with stereo
    // audio.
    WebRtc_Word32 ReadWavDataAsStereo(InStream& wav,
                                      WebRtc_Word8* audioBufferLeft,
                                      WebRtc_Word8* audioBufferRight,
                                      const WebRtc_UWord32 bufferLength);

    // Prepare for recording audio to stream.
    // codecInst specifies the encoding of the audio data.
    // Note: codecInst.channels should be set to 2 for stereo (and 1 for
    // mono). Stereo is only supported for WAV files.
    WebRtc_Word32 InitWavWriting(OutStream& stream, const CodecInst& codecInst);

    // Write one audio frame, i.e. the bufferLength first bytes of audioBuffer,
    // to file. The audio frame size is determined by the codecInst.pacsize
    // parameter of the last sucessfull StartRecordingAudioFile(..) call.
    // The return value is the number of bytes written to audioBuffer.
    WebRtc_Word32 WriteWavData(OutStream& stream,
                               const WebRtc_Word8* audioBuffer,
                               const WebRtc_UWord32 bufferLength);

    // Finalizes the WAV header so that it is correct if nothing more will be
    // written to stream.
    // Note: this API must be called before closing stream to ensure that the
    //       WAVE header is updated with the file size. Don't call this API
    //       if more samples are to be written to stream.
    WebRtc_Word32 UpdateWavHeader(OutStream& stream);

    // Prepare for playing audio from stream.
    // startPointMs and stopPointMs, unless zero, specify what part of the file
    // should be read. From startPointMs ms to stopPointMs ms.
    // freqInHz is the PCM sampling frequency.
    // NOTE, allowed frequencies are 8000, 16000 and 32000 (Hz)
    WebRtc_Word32 InitPCMReading(InStream& stream,
                                 const WebRtc_UWord32 startPointMs = 0,
                                 const WebRtc_UWord32 stopPointMs = 0,
                                 const WebRtc_UWord32 freqInHz = 16000);

    // Put 10-60ms of audio data from stream into the audioBuffer depending on
    // codec frame size. dataLengthInBytes indicates the size of audioBuffer.
    // The return value is the number of bytes written to audioBuffer.
    WebRtc_Word32 ReadPCMData(InStream& stream, WebRtc_Word8* audioBuffer,
                              const WebRtc_UWord32 dataLengthInBytes);

    // Prepare for recording audio to stream.
    // freqInHz is the PCM sampling frequency.
    // NOTE, allowed frequencies are 8000, 16000 and 32000 (Hz)
    WebRtc_Word32 InitPCMWriting(OutStream& stream,
                                 const WebRtc_UWord32 freqInHz = 16000);

    // Write one 10ms audio frame, i.e. the bufferLength first bytes of
    // audioBuffer, to file. The audio frame size is determined by the freqInHz
    // parameter of the last sucessfull InitPCMWriting(..) call.
    // The return value is the number of bytes written to audioBuffer.
    WebRtc_Word32 WritePCMData(OutStream& stream,
                               const WebRtc_Word8* audioBuffer,
                               WebRtc_UWord32 bufferLength);

    // Prepare for playing audio from stream.
    // startPointMs and stopPointMs, unless zero, specify what part of the file
    // should be read. From startPointMs ms to stopPointMs ms.
    WebRtc_Word32 InitCompressedReading(InStream& stream,
                                        const WebRtc_UWord32 startPointMs = 0,
                                        const WebRtc_UWord32 stopPointMs = 0);

    // Put 10-60ms of audio data from stream into the audioBuffer depending on
    // codec frame size. dataLengthInBytes indicates the size of audioBuffer.
    // The return value is the number of bytes written to audioBuffer.
    WebRtc_Word32 ReadCompressedData(InStream& stream,
                                     WebRtc_Word8* audioBuffer,
                                     const WebRtc_UWord32 dataLengthInBytes);

    // Prepare for recording audio to stream.
    // codecInst specifies the encoding of the audio data.
    WebRtc_Word32 InitCompressedWriting(OutStream& stream,
                                        const CodecInst& codecInst);

    // Write one audio frame, i.e. the bufferLength first bytes of audioBuffer,
    // to file. The audio frame size is determined by the codecInst.pacsize
    // parameter of the last sucessfull InitCompressedWriting(..) call.
    // The return value is the number of bytes written to stream.
    // Note: bufferLength must be exactly one frame.
    WebRtc_Word32 WriteCompressedData(OutStream& stream,
                                      const WebRtc_Word8* audioBuffer,
                                      const WebRtc_UWord32 bufferLength);

    // Prepare for playing audio from stream.
    // codecInst specifies the encoding of the audio data.
    WebRtc_Word32 InitPreEncodedReading(InStream& stream,
                                        const CodecInst& codecInst);

    // Put 10-60ms of audio data from stream into the audioBuffer depending on
    // codec frame size. dataLengthInBytes indicates the size of audioBuffer.
    // The return value is the number of bytes written to audioBuffer.
    WebRtc_Word32 ReadPreEncodedData(InStream& stream,
                                     WebRtc_Word8* audioBuffer,
                                     const WebRtc_UWord32 dataLengthInBytes);

    // Prepare for recording audio to stream.
    // codecInst specifies the encoding of the audio data.
    WebRtc_Word32 InitPreEncodedWriting(OutStream& stream,
                                        const CodecInst& codecInst);

    // Write one audio frame, i.e. the bufferLength first bytes of audioBuffer,
    // to stream. The audio frame size is determined by the codecInst.pacsize
    // parameter of the last sucessfull InitPreEncodedWriting(..) call.
   // The return value is the number of bytes written to stream.
    // Note: bufferLength must be exactly one frame.
    WebRtc_Word32 WritePreEncodedData(OutStream& stream,
                                      const WebRtc_Word8* inData,
                                      const WebRtc_UWord32 dataLengthInBytes);

    // Set durationMs to the size of the file (in ms) specified by fileName.
    // freqInHz specifies the sampling frequency of the file.
    WebRtc_Word32 FileDurationMs(const char* fileName,
                                 const FileFormats fileFormat,
                                 const WebRtc_UWord32 freqInHz = 16000);

    // Return the number of ms that have been played so far.
    WebRtc_UWord32 PlayoutPositionMs();

    // Update codecInst according to the current audio codec being used for
    // reading or writing.
    WebRtc_Word32 codec_info(CodecInst& codecInst);

private:
    // Biggest WAV frame supported is 10 ms at 48kHz of 2 channel, 16 bit audio.
    enum{WAV_MAX_BUFFER_SIZE = 480*2*2};


    WebRtc_Word32 InitWavCodec(WebRtc_UWord32 samplesPerSec,
                               WebRtc_UWord32 channels,
                               WebRtc_UWord32 bitsPerSample,
                               WebRtc_UWord32 formatTag);

    // Parse the WAV header in stream.
    WebRtc_Word32 ReadWavHeader(InStream& stream);

    // Update the WAV header. freqInHz, bytesPerSample, channels, format,
    // lengthInBytes specify characterists of the audio data.
    // freqInHz is the sampling frequency. bytesPerSample is the sample size in
    // bytes. channels is the number of channels, e.g. 1 is mono and 2 is
    // stereo. format is the encode format (e.g. PCMU, PCMA, PCM etc).
    // lengthInBytes is the number of bytes the audio samples are using up.
    WebRtc_Word32 WriteWavHeader(OutStream& stream,
                                 const WebRtc_UWord32 freqInHz,
                                 const WebRtc_UWord32 bytesPerSample,
                                 const WebRtc_UWord32 channels,
                                 const WebRtc_UWord32 format,
                                 const WebRtc_UWord32 lengthInBytes);

    // Put dataLengthInBytes of audio data from stream into the audioBuffer.
    // The return value is the number of bytes written to audioBuffer.
    WebRtc_Word32 ReadWavData(InStream& stream, WebRtc_UWord8* audioBuffer,
                              const WebRtc_UWord32 dataLengthInBytes);

    // Update the current audio codec being used for reading or writing
    // according to codecInst.
    WebRtc_Word32 set_codec_info(const CodecInst& codecInst);

    struct WAVE_FMTINFO_header
    {
        WebRtc_Word16 formatTag;
        WebRtc_Word16 nChannels;
        WebRtc_Word32 nSamplesPerSec;
        WebRtc_Word32 nAvgBytesPerSec;
        WebRtc_Word16 nBlockAlign;
        WebRtc_Word16 nBitsPerSample;
    };
    // Identifiers for preencoded files.
    enum MediaFileUtility_CodecType
    {
        kCodecNoCodec  = 0,
        kCodecIsac,
        kCodecIsacSwb,
        kCodecIsacLc,
        kCodecL16_8Khz,
        kCodecL16_16kHz,
        kCodecL16_32Khz,
        kCodecPcmu,
        kCodecPcma,
        kCodecIlbc20Ms,
        kCodecIlbc30Ms,
        kCodecG722,
        kCodecG722_1_32Kbps,
        kCodecG722_1_24Kbps,
        kCodecG722_1_16Kbps,
        kCodecG722_1c_48,
        kCodecG722_1c_32,
        kCodecG722_1c_24,
        kCodecAmr,
        kCodecAmrWb,
        kCodecG729,
        kCodecG729_1,
        kCodecG726_40,
        kCodecG726_32,
        kCodecG726_24,
        kCodecG726_16,
        kCodecSpeex8Khz,
        kCodecSpeex16Khz
    };

    // TODO (hellner): why store multiple formats. Just store either codec_info_
    //                 or _wavFormatObj and supply conversion functions.
    WAVE_FMTINFO_header _wavFormatObj;
    WebRtc_Word32 _dataSize;      // Chunk size if reading a WAV file
    // Number of bytes to read. I.e. frame size in bytes. May be multiple
    // chunks if reading WAV.
    WebRtc_Word32 _readSizeBytes;

    WebRtc_Word32 _id;

    WebRtc_UWord32 _stopPointInMs;
    WebRtc_UWord32 _startPointInMs;
    WebRtc_UWord32 _playoutPositionMs;
    WebRtc_UWord32 _bytesWritten;

    CodecInst codec_info_;
    MediaFileUtility_CodecType _codecId;

    // The amount of bytes, on average, used for one audio sample.
    WebRtc_Word32  _bytesPerSample;
    WebRtc_Word32  _readPos;

    // Only reading or writing can be enabled, not both.
    bool _reading;
    bool _writing;

    // Scratch buffer used for turning stereo audio to mono.
    WebRtc_UWord8 _tempData[WAV_MAX_BUFFER_SIZE];

#ifdef WEBRTC_MODULE_UTILITY_VIDEO
    AviFile* _aviAudioInFile;
    AviFile* _aviVideoInFile;
    AviFile* _aviOutFile;
    VideoCodec _videoCodec;
#endif
};
} // namespace webrtc
#endif // WEBRTC_MODULES_MEDIA_FILE_SOURCE_MEDIA_FILE_UTILITY_H_
