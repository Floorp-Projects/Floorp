/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "PCMFile.h"

#include <cctype>
#include <stdio.h>
#include <string.h>

#include "gtest/gtest.h"
#include "module_common_types.h"

namespace webrtc {

#define MAX_FILE_NAME_LENGTH_BYTE 500

PCMFile::PCMFile(): 
_pcmFile(NULL), 
_nSamples10Ms(160), 
_frequency(16000), 
_endOfFile(false), 
_autoRewind(false), 
_rewinded(false),
_timestamp(0),
_readStereo(false),
_saveStereo(false)
{
    _timestamp = (((WebRtc_UWord32)rand() & 0x0000FFFF) << 16) |
        ((WebRtc_UWord32)rand() & 0x0000FFFF);
}

/*
PCMFile::~PCMFile()
{
    if(_pcmFile != NULL)
    {
        fclose(_pcmFile);
        _pcmFile = NULL;
    }
}
*/

WebRtc_Word16
PCMFile::ChooseFile(
    char*       fileName, 
    WebRtc_Word16 maxLen)
{
    char tmpName[MAX_FILE_NAME_LENGTH_BYTE];
    //strcpy(_fileName, "in.pcm");
    //printf("\n\nPlease enter the input file: ");
    EXPECT_TRUE(fgets(tmpName, MAX_FILE_NAME_LENGTH_BYTE, stdin) != NULL);
    tmpName[MAX_FILE_NAME_LENGTH_BYTE-1] = '\0';
    WebRtc_Word16 n = 0;

    // removing leading spaces
    while((isspace(tmpName[n]) || iscntrl(tmpName[n])) && 
        (tmpName[n] != 0) && 
        (n < MAX_FILE_NAME_LENGTH_BYTE))
    {
        n++;
    }
    if(n > 0)
    {
        memmove(tmpName, &tmpName[n], MAX_FILE_NAME_LENGTH_BYTE - n);
    }

    //removing trailing spaces
    n = (WebRtc_Word16)(strlen(tmpName) - 1);
    if(n >= 0)
    {
        while((isspace(tmpName[n]) || iscntrl(tmpName[n])) && 
            (n >= 0))
        {
            n--;
        }
    }
    if(n >= 0)
    {
        tmpName[n + 1] = '\0';
    }

    WebRtc_Word16 len = (WebRtc_Word16)strlen(tmpName);
    if(len > maxLen)
    {
        return -1;
    }    
    if(len > 0)
    {
        strncpy(fileName, tmpName, len+1);
    }
    return 0;
}

WebRtc_Word16
PCMFile::ChooseFile(
    char*         fileName, 
    WebRtc_Word16   maxLen, 
    WebRtc_UWord16* frequencyHz)
{
    char tmpName[MAX_FILE_NAME_LENGTH_BYTE];
    //strcpy(_fileName, "in.pcm");
    //printf("\n\nPlease enter the input file: ");
    EXPECT_TRUE(fgets(tmpName, MAX_FILE_NAME_LENGTH_BYTE, stdin) != NULL);
    tmpName[MAX_FILE_NAME_LENGTH_BYTE-1] = '\0';
    WebRtc_Word16 n = 0;

    // removing leading spaces
    while((isspace(tmpName[n]) || iscntrl(tmpName[n])) && 
        (tmpName[n] != 0) && 
        (n < MAX_FILE_NAME_LENGTH_BYTE))
    {
        n++;
    }
    if(n > 0)
    {
        memmove(tmpName, &tmpName[n], MAX_FILE_NAME_LENGTH_BYTE - n);
    }

    //removing trailing spaces
    n = (WebRtc_Word16)(strlen(tmpName) - 1);
    if(n >= 0)
    {
        while((isspace(tmpName[n]) || iscntrl(tmpName[n])) && 
            (n >= 0))
        {
            n--;
        }
    }
    if(n >= 0)
    {
        tmpName[n + 1] = '\0';
    }

    WebRtc_Word16 len = (WebRtc_Word16)strlen(tmpName);
    if(len > maxLen)
    {
        return -1;
    }    
    if(len > 0)
    {
        strncpy(fileName, tmpName, len+1);
    }
    printf("Enter the sampling frequency (in Hz) of the above file [%u]: ", *frequencyHz);
    EXPECT_TRUE(fgets(tmpName, 10, stdin) != NULL);
    WebRtc_UWord16 tmpFreq = (WebRtc_UWord16)atoi(tmpName);
    if(tmpFreq > 0)
    {
        *frequencyHz = tmpFreq;
    }
    return 0;
}

void 
PCMFile::Open(
    const char*        filename,
    WebRtc_UWord16 frequency, 
    const char*  mode, 
    bool         autoRewind)
{
    if ((_pcmFile = fopen(filename, mode)) == NULL)
    {
        printf("Cannot open file %s.\n", filename);
        ADD_FAILURE() << "Unable to read file";
    }
    _frequency = frequency;
    _nSamples10Ms = (WebRtc_UWord16)(_frequency / 100);
    _autoRewind = autoRewind;
    _endOfFile = false;
    _rewinded = false;
}

WebRtc_Word32 
PCMFile::SamplingFrequency() const
{
    return _frequency;
}

WebRtc_UWord16 
PCMFile::PayloadLength10Ms() const
{
    return _nSamples10Ms;
}

WebRtc_Word32 
PCMFile::Read10MsData(
    AudioFrame& audioFrame)
{
    WebRtc_UWord16 noChannels = 1;
    if (_readStereo)
    {
        noChannels = 2;
    }

    WebRtc_Word32 payloadSize = (WebRtc_Word32)fread(audioFrame._payloadData, sizeof(WebRtc_UWord16), _nSamples10Ms*noChannels, _pcmFile);
    if (payloadSize < _nSamples10Ms*noChannels) {
        for (int k = payloadSize; k < _nSamples10Ms*noChannels; k++)
        {
            audioFrame._payloadData[k] = 0;
        }
        if(_autoRewind)
        {
            rewind(_pcmFile);
            _rewinded = true;
        }
        else
        {
            _endOfFile = true;
        }
    }
    audioFrame._payloadDataLengthInSamples = _nSamples10Ms;
    audioFrame._frequencyInHz = _frequency;
    audioFrame._audioChannel = noChannels;
    audioFrame._timeStamp = _timestamp;
    _timestamp += _nSamples10Ms;
    return _nSamples10Ms;
}

void 
PCMFile::Write10MsData(
    AudioFrame& audioFrame)
{
    if(audioFrame._audioChannel == 1)
    {
        if(!_saveStereo)
        {
            fwrite(audioFrame._payloadData, sizeof(WebRtc_UWord16), 
                audioFrame._payloadDataLengthInSamples, _pcmFile);
        }
        else
        {
            WebRtc_Word16* stereoAudio = new WebRtc_Word16[2 * 
                audioFrame._payloadDataLengthInSamples];
            int k;
            for(k = 0; k < audioFrame._payloadDataLengthInSamples; k++)
            {
                stereoAudio[k<<1] = audioFrame._payloadData[k];
                stereoAudio[(k<<1) + 1] = audioFrame._payloadData[k];
            }
            fwrite(stereoAudio, sizeof(WebRtc_Word16), 2*audioFrame._payloadDataLengthInSamples,
                _pcmFile);
            delete [] stereoAudio;
        }
    }
    else
    {
        fwrite(audioFrame._payloadData, sizeof(WebRtc_Word16), 
            audioFrame._audioChannel * audioFrame._payloadDataLengthInSamples, _pcmFile);
    }
}


void 
PCMFile::Write10MsData(
    WebRtc_Word16* playoutBuffer, 
    WebRtc_UWord16 playoutLengthSmpls)
{
    fwrite(playoutBuffer, sizeof(WebRtc_UWord16), playoutLengthSmpls, _pcmFile);
}


void 
PCMFile::Close()
{
    fclose(_pcmFile);
    _pcmFile = NULL;
}

void 
PCMFile::Rewind()
{
    rewind(_pcmFile);
    _endOfFile = false;
}

bool 
PCMFile::Rewinded()
{
    return _rewinded;
}

void
PCMFile::SaveStereo(
    bool saveStereo)
{
    _saveStereo = saveStereo;
}

void
PCMFile::ReadStereo(
    bool readStereo)
{
    _readStereo = readStereo;
}

} // namespace webrtc
