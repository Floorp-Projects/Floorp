/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PCMFILE_H
#define PCMFILE_H

#include "typedefs.h"
#include "module_common_types.h"
#include <cstdio>
#include <cstdlib>

namespace webrtc {

class PCMFile
{
public:
    PCMFile();
    ~PCMFile()
    {
        if(_pcmFile != NULL)
        {
            fclose(_pcmFile);
        }
    }
    void Open(const char *filename, WebRtc_UWord16 frequency, const char *mode,
              bool autoRewind = false);
    
    WebRtc_Word32 Read10MsData(AudioFrame& audioFrame);
    
    void Write10MsData(WebRtc_Word16 *playoutBuffer, WebRtc_UWord16 playoutLengthSmpls);
    void Write10MsData(AudioFrame& audioFrame);

    WebRtc_UWord16 PayloadLength10Ms() const;
    WebRtc_Word32 SamplingFrequency() const;
    void Close();
    bool EndOfFile() const { return _endOfFile; }
    void Rewind();
    static WebRtc_Word16 ChooseFile(char* fileName, WebRtc_Word16 maxLen,
                                    WebRtc_UWord16* frequencyHz);
    static WebRtc_Word16 ChooseFile(char* fileName, WebRtc_Word16 maxLen);
    bool Rewinded();
    void SaveStereo(
        bool saveStereo = true);
    void ReadStereo(
        bool readStereo = true);
private:
    FILE*           _pcmFile;
    WebRtc_UWord16  _nSamples10Ms;
    WebRtc_Word32   _frequency;
    bool            _endOfFile;
    bool            _autoRewind;
    bool            _rewinded;
    WebRtc_UWord32  _timestamp;
    bool            _readStereo;
    bool            _saveStereo;
};

} // namespace webrtc

#endif
