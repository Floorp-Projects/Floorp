/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "dtmf_inband.h"

#include "critical_section_wrapper.h"
#include "trace.h"
#include <cassert>

namespace webrtc {

const WebRtc_Word16 Dtmf_a_times2Tab8Khz[8]=
{
	27978, 26956, 25701, 24219,
	19073, 16325, 13085, 9314
};

const WebRtc_Word16 Dtmf_a_times2Tab16Khz[8]=
{
	31548, 31281, 30951, 30556,
	29144, 28361, 27409, 26258
};

const WebRtc_Word16 Dtmf_a_times2Tab32Khz[8]=
{
	32462,32394, 32311, 32210, 31849, 31647, 31400, 31098
};

// Second table is sin(2*pi*f/fs) in Q14

const WebRtc_Word16 Dtmf_ym2Tab8Khz[8]=
{
	8527, 9315, 10163, 11036,
	13322, 14206, 15021, 15708
};

const WebRtc_Word16 Dtmf_ym2Tab16Khz[8]=
{
	4429, 4879, 5380, 5918,
	7490, 8207, 8979, 9801
};

const WebRtc_Word16 Dtmf_ym2Tab32Khz[8]=
{
	2235, 2468, 2728, 3010, 3853, 4249, 4685, 5164
};

const WebRtc_Word16 Dtmf_dBm0kHz[37]=
{
       16141,      14386,      12821,      11427,      10184,       9077,
        8090,       7210,       6426,       5727,       5104,       4549,
        4054,       3614,       3221,       2870,       2558,       2280,
        2032,       1811,       1614,       1439,       1282,       1143,
        1018,        908,        809,        721,        643,        573,
         510,        455,        405,        361,        322,        287,
		 256
};


DtmfInband::DtmfInband(const WebRtc_Word32 id) :
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _id(id),
    _outputFrequencyHz(8000),
    _frameLengthSamples(0),
    _remainingSamples(0),
    _eventCode(0),
    _attenuationDb(0),
    _lengthMs(0),
    _reinit(true),
    _playing(false),
    _delaySinceLastToneMS(1000)
{
    memset(_oldOutputLow, 0, sizeof(_oldOutputLow));
    memset(_oldOutputHigh, 0, sizeof(_oldOutputHigh));
}

DtmfInband::~DtmfInband()
{
	delete &_critSect;
}

int
DtmfInband::SetSampleRate(const WebRtc_UWord16 frequency)
{
    if (frequency != 8000 &&
            frequency != 16000 &&
            frequency != 32000)
    {
        // invalid sample rate
        assert(false);
        return -1;
    }
    _outputFrequencyHz = frequency;
    return 0;
}

int
DtmfInband::GetSampleRate(WebRtc_UWord16& frequency)
{
    frequency = _outputFrequencyHz;
    return 0;
}

void 
DtmfInband::Init()
{
    _remainingSamples = 0;
    _frameLengthSamples = 0;
    _eventCode = 0;
    _attenuationDb = 0;
    _lengthMs = 0;
    _reinit = true;
    _oldOutputLow[0] = 0;
    _oldOutputLow[1] = 0;
    _oldOutputHigh[0] = 0;
    _oldOutputHigh[1] = 0;
    _delaySinceLastToneMS = 1000;
}

int
DtmfInband::AddTone(const WebRtc_UWord8 eventCode,
                    WebRtc_Word32 lengthMs,
                    WebRtc_Word32 attenuationDb)
{
    CriticalSectionScoped lock(&_critSect);

    if (attenuationDb > 36 || eventCode > 15)
    {
        assert(false);
        return -1;
    }

    if (IsAddingTone())
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_id,-1),
                   "DtmfInband::AddTone() new tone interrupts ongoing tone");
    }

    ReInit();

    _frameLengthSamples = static_cast<WebRtc_Word16> (_outputFrequencyHz / 100);
    _eventCode = static_cast<WebRtc_Word16> (eventCode);
    _attenuationDb = static_cast<WebRtc_Word16> (attenuationDb);
    _remainingSamples = static_cast<WebRtc_Word32>
        (lengthMs * (_outputFrequencyHz / 1000));
    _lengthMs = lengthMs;

    return 0;
}

int
DtmfInband::ResetTone()
{
    CriticalSectionScoped lock(&_critSect);

    ReInit();

    _frameLengthSamples = static_cast<WebRtc_Word16> (_outputFrequencyHz / 100);
    _remainingSamples = static_cast<WebRtc_Word32>
        (_lengthMs * (_outputFrequencyHz / 1000));

    return 0;
}

int
DtmfInband::StartTone(const WebRtc_UWord8 eventCode,
                      WebRtc_Word32 attenuationDb)
{
    CriticalSectionScoped lock(&_critSect);

    if (attenuationDb > 36 || eventCode > 15)
    {
        assert(false);
        return -1;
    }

    if (IsAddingTone())
    {
            return -1;
    }

    ReInit();

    _frameLengthSamples = static_cast<WebRtc_Word16> (_outputFrequencyHz / 100);
    _eventCode = static_cast<WebRtc_Word16> (eventCode);
    _attenuationDb = static_cast<WebRtc_Word16> (attenuationDb);
    _playing = true;

    return 0;
}

int
DtmfInband::StopTone()
{
    CriticalSectionScoped lock(&_critSect);

    if (!_playing)
    {
        return 0;
    }

    _playing = false;

    return 0;
}

// Shall be called between tones
void 
DtmfInband::ReInit()
{
    _reinit = true;
}

bool 
DtmfInband::IsAddingTone()
{
    CriticalSectionScoped lock(&_critSect);
    return (_remainingSamples > 0 || _playing);
}

int
DtmfInband::Get10msTone(WebRtc_Word16 output[320],
                        WebRtc_UWord16& outputSizeInSamples)
{
    CriticalSectionScoped lock(&_critSect);
    if (DtmfFix_generate(output,
                         _eventCode,
                         _attenuationDb,
                         _frameLengthSamples,
                         _outputFrequencyHz) == -1)
    {
        return -1;
    }
    _remainingSamples -= _frameLengthSamples;
    outputSizeInSamples = _frameLengthSamples;
    _delaySinceLastToneMS = 0;
    return 0;
}

void
DtmfInband::UpdateDelaySinceLastTone()
{
    _delaySinceLastToneMS += kDtmfFrameSizeMs;
    // avoid wraparound
    if (_delaySinceLastToneMS > (1<<30))
    {
        _delaySinceLastToneMS = 1000;
    }
}

WebRtc_UWord32
DtmfInband::DelaySinceLastTone() const
{
    return _delaySinceLastToneMS;
}

WebRtc_Word16
DtmfInband::DtmfFix_generate(WebRtc_Word16 *decoded,
                             const WebRtc_Word16 value,
                             const WebRtc_Word16 volume,
                             const WebRtc_Word16 frameLen,
                             const WebRtc_Word16 fs)
{
    const WebRtc_Word16 *a_times2Tbl;
    const WebRtc_Word16 *y2_Table;
    WebRtc_Word16 a1_times2 = 0, a2_times2 = 0;

    if (fs==8000) {
        a_times2Tbl=Dtmf_a_times2Tab8Khz;
        y2_Table=Dtmf_ym2Tab8Khz;
    } else if (fs==16000) {
        a_times2Tbl=Dtmf_a_times2Tab16Khz;
        y2_Table=Dtmf_ym2Tab16Khz;
    } else if (fs==32000) {
        a_times2Tbl=Dtmf_a_times2Tab32Khz;
        y2_Table=Dtmf_ym2Tab32Khz;
    } else {
        return(-1);
    }

    if ((value==1)||(value==2)||(value==3)||(value==12)) {
        a1_times2=a_times2Tbl[0];
        if (_reinit) {
            _oldOutputLow[0]=y2_Table[0];
            _oldOutputLow[1]=0;
        }
    } else if ((value==4)||(value==5)||(value==6)||(value==13)) {
        a1_times2=a_times2Tbl[1];
        if (_reinit) {
            _oldOutputLow[0]=y2_Table[1];
            _oldOutputLow[1]=0;
        }
    } else if ((value==7)||(value==8)||(value==9)||(value==14)) {
        a1_times2=a_times2Tbl[2];
        if (_reinit) {
            _oldOutputLow[0]=y2_Table[2];
            _oldOutputLow[1]=0;
        }
    } else if ((value==10)||(value==0)||(value==11)||(value==15)) {
        a1_times2=a_times2Tbl[3];
        if (_reinit) {
            _oldOutputLow[0]=y2_Table[3];
            _oldOutputLow[1]=0;
        }
    }
    if ((value==1)||(value==4)||(value==7)||(value==10)) {
        a2_times2=a_times2Tbl[4];
        if (_reinit) {
            _oldOutputHigh[0]=y2_Table[4];
            _oldOutputHigh[1]=0;
            _reinit=false;
        }
    } else if ((value==2)||(value==5)||(value==8)||(value==0)) {
        a2_times2=a_times2Tbl[5];
        if (_reinit) {
            _oldOutputHigh[0]=y2_Table[5];
            _oldOutputHigh[1]=0;
            _reinit=false;
        }
    } else if ((value==3)||(value==6)||(value==9)||(value==11)) {
        a2_times2=a_times2Tbl[6];
        if (_reinit) {
            _oldOutputHigh[0]=y2_Table[6];
            _oldOutputHigh[1]=0;
            _reinit=false;
        }
    } else if ((value==12)||(value==13)||(value==14)||(value==15)) {
        a2_times2=a_times2Tbl[7];
        if (_reinit) {
            _oldOutputHigh[0]=y2_Table[7];
            _oldOutputHigh[1]=0;
            _reinit=false;
        }
    }

    return (DtmfFix_generateSignal(a1_times2,
                                   a2_times2,
                                   volume,
                                   decoded,
                                   frameLen));
}

WebRtc_Word16
DtmfInband::DtmfFix_generateSignal(const WebRtc_Word16 a1_times2,
                                   const WebRtc_Word16 a2_times2,
                                   const WebRtc_Word16 volume,
                                   WebRtc_Word16 *signal,
                                   const WebRtc_Word16 length)
{
    int i;

    /* Generate Signal */
    for (i=0;i<length;i++) {
        WebRtc_Word32 tempVal;
        WebRtc_Word16 tempValLow, tempValHigh;

        /* Use recursion formula y[n] = a*2*y[n-1] - y[n-2] */
        tempValLow  = (WebRtc_Word16)(((( (WebRtc_Word32)(a1_times2 *
            _oldOutputLow[1])) + 8192) >> 14) - _oldOutputLow[0]);
        tempValHigh = (WebRtc_Word16)(((( (WebRtc_Word32)(a2_times2 *
            _oldOutputHigh[1])) + 8192) >> 14) - _oldOutputHigh[0]);

        /* Update memory */
        _oldOutputLow[0]=_oldOutputLow[1];
        _oldOutputLow[1]=tempValLow;
        _oldOutputHigh[0]=_oldOutputHigh[1];
        _oldOutputHigh[1]=tempValHigh;

        tempVal = (WebRtc_Word32)(kDtmfAmpLow * tempValLow) +
            (WebRtc_Word32)(kDtmfAmpHigh * tempValHigh);

        /* Norm the signal to Q14 */
        tempVal=(tempVal+16384)>>15;

        /* Scale the signal to correct dbM0 value */
        signal[i]=(WebRtc_Word16)((tempVal*Dtmf_dBm0kHz[volume]+8192)>>14);
    }

    return(0);
}

}  // namespace webrtc
