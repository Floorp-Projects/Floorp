/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _USE_CPVE

#include "CC_Common.h"
#include "WebrtcLogging.h"

#include "WebrtcAudioCodecSelector.h"
#include "voe_codec.h"
#include "csf_common.h"

static const char* logTag = "AudioCodecSelector";

using namespace std;
using namespace CSF;

typedef struct _CSFCodecMapping
{
    AudioPayloadType csfAudioPayloadType;
    WebrtcAudioPayloadType webrtcAudioPayloadType;
} CSFCodecMapping;

static CSFCodecMapping WebrtcMappingInfo[] =
{
    { AudioPayloadType_G711ALAW64K,       WebrtcAudioPayloadType_PCMA },
    { AudioPayloadType_G711ALAW56K,       WebrtcAudioPayloadType_PCMA },
    { AudioPayloadType_G711ULAW64K,       WebrtcAudioPayloadType_PCMU },
    { AudioPayloadType_G711ULAW56K,       WebrtcAudioPayloadType_PCMU },
    { AudioPayloadType_G722_56K,          WebrtcAudioPayloadType_G722 },
    { AudioPayloadType_G722_64K,          WebrtcAudioPayloadType_G722 },
    { AudioPayloadType_G722_48K,          WebrtcAudioPayloadType_G722 },
    { AudioPayloadType_RFC2833,           WebrtcAudioPayloadType_TELEPHONE_EVENT }
};

WebrtcAudioCodecSelector::WebrtcAudioCodecSelector()
    : voeCodec(NULL)
{
    LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::constructor" );
}

WebrtcAudioCodecSelector::~WebrtcAudioCodecSelector()
{
    LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::destructor" );
    release();
}

void WebrtcAudioCodecSelector::release()
{
    LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::release" );

    // release the VoE Codec sub-interface
    if (voeCodec != NULL)
    {
        if (voeCodec->Release() != 0)
        {
            LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::release voeCodec->Release() failed" );
        }
        else
        {
            LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::release voeCodec released" );
        }

        voeCodec = NULL;

        std::map<int, webrtc::CodecInst*>::iterator iterVoeCodecs;
        for( iterVoeCodecs = codecMap.begin(); iterVoeCodecs != codecMap.end(); ++iterVoeCodecs ) {
            delete iterVoeCodecs->second;
        }

        codecMap.clear();
    }
}

int WebrtcAudioCodecSelector::init( webrtc::VoiceEngine* voeVoice, bool useLowBandwidthCodecOnly, bool advertiseG722Codec )
{
    LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::init useLowBandwidthCodecOnly=%d, advertiseG722Codec=%d", useLowBandwidthCodecOnly, advertiseG722Codec );

    voeCodec = webrtc::VoECodec::GetInterface( voeVoice );

    if (voeCodec == NULL)
    {
        LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::init cannot get reference to VOE codec interface" );
        return -1;
    }

    // clear the existing codec map
    LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::init clearing map" );
    codecMap.clear();

    // get the number of codecs supported by VOE
    int numOfSupportedCodecs = voeCodec->NumOfCodecs();

    LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::init found %d supported codec(s)", numOfSupportedCodecs );

    // iterate over supported codecs
    for (int codecIndex = 0; codecIndex < numOfSupportedCodecs; codecIndex++)
    {
        webrtc::CodecInst supportedCodec;

        if (voeCodec->GetCodec(codecIndex, supportedCodec) == -1)
        {
            LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::init codecIndex=%d: cannot get supported codec information", codecIndex );
            continue;
        }

        LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::init codecIndex=%d: channels=%d, pacsize=%d, plfreq=%d, plname=%s, pltype=%d, rate=%d",
                codecIndex, supportedCodec.channels, supportedCodec.pacsize, supportedCodec.plfreq,
                supportedCodec.plname, supportedCodec.pltype, supportedCodec.rate );

        // iterate over the payload conversion table
        for (int i=0; i< (int) csf_countof(WebrtcMappingInfo); i++)
        {
            WebrtcAudioPayloadType webrtcPayload = WebrtcMappingInfo[i].webrtcAudioPayloadType;

            if (supportedCodec.pltype == webrtcPayload)
            {
                bool addCodec = false;

                AudioPayloadType csfPayload = WebrtcMappingInfo[i].csfAudioPayloadType;

                switch (csfPayload)
                {
                case AudioPayloadType_G711ALAW64K:
                case AudioPayloadType_G711ULAW64K:
                    if (!useLowBandwidthCodecOnly)
                    {
                        addCodec = true;
                    }
                    break;

                case AudioPayloadType_G722_56K:
                case AudioPayloadType_G722_64K:
                    if (!useLowBandwidthCodecOnly &&
                        advertiseG722Codec)
                    {
                        addCodec =  true;
                    }
                    break;

                // iLBC and iSAC support is postponed
                //case AudioPayloadType_ILBC20:
                //case AudioPayloadType_ILBC30:
                //    addCodec =  true;
                //    break;
                //
                //case AudioPayloadType_ISAC:
                //    addCodec = true;
                //    break;

                case AudioPayloadType_RFC2833:
                    addCodec =  true;
                    break;

                case AudioPayloadType_G711ALAW56K:
                case AudioPayloadType_G711ULAW56K:
                case AudioPayloadType_G722_48K:
                case AudioPayloadType_ILBC20:
                case AudioPayloadType_ILBC30:
                case AudioPayloadType_ISAC:
                  break;
                    
                } // end of switch(csfPayload)

                if (addCodec)
                {
                    // add to codec map
                    webrtc::CodecInst* mappedCodec = new webrtc::CodecInst; // not sure when it should be deleted
                    memcpy(mappedCodec, &supportedCodec, sizeof(webrtc::CodecInst));

                    codecMap[csfPayload] = mappedCodec;

                    LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::init added mapping payload %d to VoE codec %s", csfPayload, mappedCodec->plname);
                }
                else
                {
                    LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::init no mapping found for VoE codec %s (payload %d)", supportedCodec.plname, webrtcPayload );
                }
            }
        } // end of iteration over the payload conversion table

    } // end of iteration over supported codecs

    LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::init %d codec(s) added to map", (int)codecMap.size() );

    // return success
    return 0;
}

int  WebrtcAudioCodecSelector::advertiseCodecs( CodecRequestType requestType )
{
    return AudioCodecMask_G711  |AudioCodecMask_G722;
}

int WebrtcAudioCodecSelector::select( int payloadType, int dynamicPayloadType, int packPeriod, webrtc::CodecInst& selectedCodec )
{
    LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::select payloadType=%d, dynamicPayloadType=%d, packPeriod=%d", payloadType, dynamicPayloadType, packPeriod );

    // TO DO: calculate packet size ?
    // packPeriod "represents the number of milliseconds of audio encoded in a single packet" ?
    int packetSize = packPeriod;

    webrtc::CodecInst* supportedCodec = codecMap[payloadType];

    if (supportedCodec == NULL)
    {
        LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::select no VoE codec found for payload %d", payloadType);
        return -1; // return failure
    }

    memcpy(&selectedCodec, supportedCodec, sizeof(webrtc::CodecInst));

    // adapt dynamic payload type if required
    if (dynamicPayloadType != -1)
    {
        selectedCodec.pltype = dynamicPayloadType;
        LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::select pltype = %d", selectedCodec.pltype);
    }

    // adapt packet size
    int pacsize;    // packet size in samples = sample rate * packet size

    switch ( payloadType )
    {
    case AudioPayloadType_G711ALAW64K:
    case AudioPayloadType_G711ULAW64K:
        // VoE allowed packet sizes for G.711 u/a law: 10, 20, 30, 40, 50, 60 ms
        // (or 80, 160, 240, 320, 400, 480 samples)
        pacsize = ( 8 * packetSize );
        if (( pacsize == 80 ) || ( pacsize == 160 ) ||
            ( pacsize == 240) || ( pacsize == 320 ) ||
            ( pacsize == 400) || ( pacsize == 480 ))
        {
            selectedCodec.pacsize = pacsize;
        }
        break;

    case AudioPayloadType_ILBC20:
        // VoE allowed packet sizes for iLBC: 20 30, 40, 60 ms
        pacsize = ( 8 * packetSize );
        if ( pacsize == 160 )
        {
            selectedCodec.pacsize = pacsize;
        }
        break;

    case AudioPayloadType_ILBC30:
        // VoE allowed packet sizes for iLBC: 20 30, 40, 60 ms
        pacsize = ( 8 * packetSize );
        if ( pacsize == 240 )
        {
            selectedCodec.pacsize = pacsize;
        }
        break;

    case AudioPayloadType_G722_56K:
    case AudioPayloadType_G722_64K:
        // VoE allowed packet size for G.722: 20ms (or 320 samples).
        pacsize = ( 16 * packetSize );
        if ( pacsize == 320 )
        {
            selectedCodec.pacsize = pacsize;
        }
        break;

    default:
        break;
    }

    LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::select found codec %s (payload=%d, packetSize=%d)",
            selectedCodec.plname, selectedCodec.pltype, selectedCodec.pacsize);

    // return success
    return 0;
}

int WebrtcAudioCodecSelector::setSend(int channel, const webrtc::CodecInst& codec,int payloadType,bool vad)
{
    webrtc::PayloadFrequencies freq;
    LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::setSend channel=%d codec %s (payload=%d, packetSize=%d)",
            channel, codec.plname, codec.pltype, codec.pacsize);

    if (voeCodec == NULL)
    {
        LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::setSend voeCodec is null" );
        return -1;
    }
    bool vadEnable=vad;

    if (voeCodec->SetVADStatus( channel,  vadEnable,  webrtc::kVadConventional,false) != 0)
    {
        LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::SetVADStatus cannot set VAD  to channel %d", channel );
        return -1;
    }
    
    // Check the codec's sampling  frequency and use appropriate enum 
    switch (codec.plfreq)
    {
        case SamplingFreq8000Hz:
            freq=webrtc::kFreq8000Hz;
        break;
        case SamplingFreq16000Hz:
            freq=webrtc::kFreq16000Hz;
            break;
       case SamplingFreq32000Hz:
            freq=webrtc::kFreq32000Hz;
            break;            
       default:
            freq=webrtc::kFreq8000Hz;

    }
    
    if (voeCodec->SetSendCNPayloadType(channel, ComfortNoisePayloadType,  freq) != 0)
    {
        LOG_WEBRTC_INFO( logTag, "SetSendCNPayloadType cannot set CN payload type  to channel %d", channel );
        //return -1;
    }
    // apply the codec to the channel
    if (voeCodec->SetSendCodec(channel, codec) != 0)
    {
        LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::setSend cannot set send codec to channel %d", channel );
        return -1;
    }

    
    LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::setSend applied codec %s (payload=%d, packetSize=%d) to channel %d",
            codec.plname, codec.pltype, codec.pacsize, channel);

    // return success
    return 0;
}

int WebrtcAudioCodecSelector::setReceive(int channel, const webrtc::CodecInst& codec)
{
    LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::setReceive channel=%d codec %s (payload=%d, packetSize=%d)",
            channel, codec.plname, codec.pltype, codec.pacsize);

    if (voeCodec == NULL)
    {
        LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::setSend voeCodec is null" );
        return -1;
    }

    if (voeCodec->SetRecPayloadType(channel, codec) != 0)
    {
        LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::setReceive cannot set receive codec to channel %d", channel );
        return -1;
    }

    LOG_WEBRTC_INFO( logTag, "WebrtcAudioCodecSelector::setReceive applied codec %s (payload=%d, packetSize=%d) to channel %d",
            codec.plname, codec.pltype, codec.pacsize, channel);

    // return success
    return 0;
}

#endif
