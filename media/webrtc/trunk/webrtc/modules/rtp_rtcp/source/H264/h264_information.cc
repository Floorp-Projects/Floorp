/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>
#include "h264_information.h"

//#define DEBUG_SEI_MESSAGE 1

#ifdef DEBUG_SEI_MESSAGE
    #include "bitstream_parser.h"
    #include <stdio.h>
    #include <math.h>

    uint32_t BitRateBPS(uint16_t x )
    {
        return (x & 0x3fff) * uint32_t(pow(10.0f,(2 + (x >> 14))));
    }

#endif

namespace webrtc {
H264Information::H264Information(const bool SVC)
    : _SVC(SVC)

{
}

H264Information::~H264Information()
{

}

void
H264Information::Reset()
{
    _parsedLength = 0;
    _remLength = 0;
    _length = 0;
    _info.numNALUs = 0;
    _info.numLayers = 0;

    memset(_info.startCodeSize, 0, sizeof(_info.startCodeSize));
    memset(_info.payloadSize, 0, sizeof(_info.payloadSize));
    memset(_info.NRI, 0, sizeof(_info.NRI));
    memset(_info.type, 0, sizeof(_info.type));
    memset(_info.accLayerSize, 0, sizeof(_info.accLayerSize));

    for (int32_t i = 0; i < KMaxNumberOfNALUs; i++)
    {
        _info.SVCheader[i].idr =            0;
        _info.SVCheader[i].priorityID =     0;
        _info.SVCheader[i].interLayerPred = 0;
        _info.SVCheader[i].dependencyID =   0;
        _info.SVCheader[i].qualityID =      0;
        _info.SVCheader[i].temporalID =     0;
        _info.SVCheader[i].useRefBasePic =  0;
        _info.SVCheader[i].discardable =    0;
        _info.SVCheader[i].output =         0;

        _info.PACSI[i].X = 0;
        _info.PACSI[i].Y = 0;
//      _info.PACSI[i].T = 0;
        _info.PACSI[i].A = 0;
        _info.PACSI[i].P = 0;
        _info.PACSI[i].C = 0;
        _info.PACSI[i].S = 0;
        _info.PACSI[i].E = 0;
        _info.PACSI[i].TL0picIDx =   0;
        _info.PACSI[i].IDRpicID =    0;
        _info.PACSI[i].DONC =        0;
        _info.PACSI[i].numSEINALUs = 0;
        _info.PACSI[i].NALlength =   5;
    }
}

/*******************************************************************************
 * int32_t GetInfo(const uint8_t* ptrEncodedBuffer,
 *             const uint32_t length,
 *             const H264Info*& ptrInfo);
 *
 * Gets information from an encoded stream.
 *
 * Input:
 *          - ptrEncodedBuffer  : Pointer to encoded stream.
 *          - length            : Length in bytes of encoded stream.
 *
 * Output:
 *          - ptrInfo           : Pointer to struct with H.264 info.
 *
 * Return value:
 *          - 0                 : ok
 *          - (-1)              : Error
 */
int32_t
H264Information::GetInfo(const uint8_t* ptrEncodedBuffer,
                             const uint32_t length,
                             const H264Info*& ptrInfo)
{
    if (!ptrEncodedBuffer || length < 4)
    {
        return -1;
    }

    if (!HasInfo(length))
    {
        if (-1 == FindInfo(ptrEncodedBuffer, length))
        {
            Reset();
            return -1;
        }
    }
    ptrInfo = &_info;
    return 0;
}

RtpVideoCodecTypes
H264Information::Type()
{
    if(_SVC)
    {
        return RTP_H264_SVCVideo;
    }
    return RTP_H264Video;
}


/*******************************************************************************
 * bool HasInfo(const uint32_t length);
 *
 * Checks if information has already been stored for this encoded stream.
 *
 * Input:
 *          - length            : Length in bytes of encoded stream.
 *
 * Return value:
 *          - true (false)      : Information has (not) been stored.
 */

bool
H264Information::HasInfo(const uint32_t length)
{
    if (!_info.numNALUs)
    {
        return false;
    }

    // has info, make sure current length matches info length
    if (length != _length)
    {
        Reset();
        return false;
    }

    return true;
}

/*******************************************************************************
 * int32_t FindInfo(const uint8_t* ptrEncodedBuffer,
 *              const uint32_t length);
 *
 * Parses the encoded stream.
 *
 * Input:
 *          - ptrEncodedBuffer  : Pointer to encoded stream.
 *          - length            : Length in bytes of encoded stream.
 *
 * Return value:
 *          - 0                 : ok
 *          - (-1)              : Error
 */
int32_t
H264Information::FindInfo(const uint8_t* ptrEncodedBuffer, const uint32_t length)
{
    _ptrData = ptrEncodedBuffer;
    _length = length;
    _parsedLength = 0;
    _remLength = length;

    do
    {
        // Get start code length
        if (FindNALUStartCodeSize() == -1)
        {
            Reset();
            return -1;
        }

        // Get NAL unit payload size
        int32_t foundLast = FindNALU();
        if (foundLast == -1)
        {
            Reset();
            return -1;
        }

        // Validate parsed length
        if (_parsedLength > _length)
        {
            Reset();
            return -1;
        }

        // Get NRI
        GetNRI();

        // Get type
        if (FindNALUType() == -1)
        {
            Reset();
            return -1;
        }

        // Set layer start end bit
        SetLayerSEBit(foundLast);


        // Last NAL unit found?
        if (foundLast == 1)
        {
            if (_parsedLength != _length)
            {
                Reset();
                return -1;
            }
            _info.numNALUs++;
            return SetLayerLengths();
        }

        // Next NAL unit
        _ptrData   += (_info.startCodeSize[_info.numNALUs] + _info.payloadSize[_info.numNALUs]);
        _remLength -= (_info.startCodeSize[_info.numNALUs] + _info.payloadSize[_info.numNALUs]);
        _info.numNALUs++;

        // Validate memory allocation
        if (_info.numNALUs >= KMaxNumberOfNALUs)
        {
            Reset();
            return -1;
        }
    }
    while(true);

    return 0;
}

/*******************************************************************************
 * int32_t FindNALUStartCodeSize();
 *
 * Finds the start code length of the current NAL unit.
 *
 * Output:
 *          - _info.startCodeSize[currentNALU]  : Start code length in bytes of NAL unit.
 *
 * Return value:
 *          - 0                 : ok
 *          - (-1)              : Error
 */
int32_t
H264Information::FindNALUStartCodeSize()
{
    // NAL unit start code. Ex. {0,0,1} or {0,0,0,1}
    for (uint32_t i = 2; i < _remLength; i++)
    {
        if (_ptrData[i] == 1 && _ptrData[i - 1] == 0 && _ptrData[i - 2] == 0)
        {
            _info.startCodeSize[_info.numNALUs] = uint8_t(i + 1);
            return 0;
        }
    }
    return -1;
}

/*******************************************************************************
 * int32_t FindNALU();
 *
 * Finds the length of the current NAL unit.
 *
 * Output:
 *          - _info.payloadSize[currentNALU]  : Payload length in bytes of NAL unit
 *                                              (start code length not included).
 *          - _parsedLength                   : Current parsed length in bytes.
 *
 * Return value:
 *          - 1                 : ok. Last NAL unit found.
 *          - 0                 : ok
 *          - (-1)              : Error
 */
int32_t
H264Information::FindNALU()
{
    for (uint32_t i = _info.startCodeSize[_info.numNALUs]; i < _remLength - 2; i += 2)
    {
        if (_ptrData[i] == 0)
        {
            int32_t size = 0;
            if ((_ptrData[i + 1] == 1 && _ptrData[i - 1] == 0) ||
                (_ptrData[i + 2] == 1 && _ptrData[i + 1] == 0))
            {
                // Found a header
                // Reduce size by preceding zeroes
                while (_ptrData[i - 1] == 0)
                {
                    i--;
                }
                size = i;
            }
            if (size > 0)
            {
                _info.payloadSize[_info.numNALUs] = size - _info.startCodeSize[_info.numNALUs];
                _parsedLength += _info.startCodeSize[_info.numNALUs] + _info.payloadSize[_info.numNALUs];
                return 0;
            }
        }
    }
    // Last NAL unit
    _info.payloadSize[_info.numNALUs] = _remLength - _info.startCodeSize[_info.numNALUs];
    if (_info.payloadSize[_info.numNALUs] > 0)
    {
        _parsedLength += _info.startCodeSize[_info.numNALUs] + _info.payloadSize[_info.numNALUs];
        return 1;
    }
    return -1;
}

/*******************************************************************************
 * void GetNRI();
 *
 * Finds the NRI of the current NAL unit.
 *
 * Output:
 *          - _info.NRI[currentNALU]   : NRI of NAL unit.
 *
 * Return value:
 *          - 0                        : ok
 *          - (-1)                     : Error
 */
void
H264Information::GetNRI()
{
    //  NAL unit header (1 byte)
    //  ---------------------------------
    // |   start code    |F|NRI|  Type   |
    //  ---------------------------------

    // NRI (2 bits) - nal_ref_idc. '00' - the NAL unit is not used to reconstruct reference pictures.
    //                             >00  - the NAL unit is required to reconstruct reference pictures
    //                                    in the same layer, or contains a parameter set.


    const uint8_t type = _ptrData[_info.startCodeSize[_info.numNALUs]] & 0x1f;

    // NALU type of 5, 7 and 8 shoud have NRI to b011
    if( type == 5 ||
        type == 7 ||
        type == 8)
    {
        _info.NRI[_info.numNALUs] = 0x60;
    }else
    {
        _info.NRI[_info.numNALUs] = _ptrData[_info.startCodeSize[_info.numNALUs]] & 0x60;
    }
}


/*******************************************************************************
 * int32_t FindNALUType();
 *
 * Finds the type of the current NAL unit.
 *
 * Output:
 *          - _info.type[currentNALU]  : Type of NAL unit
 *
 * Return value:
 *          - 0                        : ok
 *          - (-1)                     : Error
 */
int32_t
H264Information::FindNALUType()
{
    //  NAL unit header (1 byte)
    //  ---------------------------------
    // |   start code    |F|NRI|  Type   |
    //  ---------------------------------

    _info.type[_info.numNALUs] = _ptrData[_info.startCodeSize[_info.numNALUs]] & 0x1f;

    if (_info.type[_info.numNALUs] == 0)
    {
        return -1;
    }

    // SVC NAL units, extended header
    if (ParseSVCNALUHeader() == -1)
    {
        return -1;
    }

    return 0;
}

/*******************************************************************************
 * int32_t ParseSVCNALUHeader();
 *
 * Finds the extended header of the current NAL unit. Included for NAL unit types 14 and 20.
 *
 * Output:
 *          - _info.SVCheader[currentNALU]  : SVC header of NAL unit.
 *
 * Return value:
 *          - 0                             : ok
 *          - (-1)                          : Error
 */
int32_t
H264Information::ParseSVCNALUHeader()
{
    if (_info.type[_info.numNALUs] == 5)
    {
        _info.SVCheader[_info.numNALUs].idr = 1;
    }
    if (_info.type[_info.numNALUs] == 6)
    {
        uint32_t seiPayloadSize;
        do
        {
            // SEI message
            seiPayloadSize = 0;

            uint32_t curByte = _info.startCodeSize[_info.numNALUs] + 1;
            const uint32_t seiStartOffset = curByte;

            uint32_t seiPayloadType = 0;
            while(_ptrData[curByte] == 0xff)
            {
                seiPayloadType += 255;
                curByte++;
            }
            seiPayloadType += _ptrData[curByte++];

            while(_ptrData[curByte] == 0xff)
            {
                seiPayloadSize += 255;
                curByte++;
            }
            seiPayloadSize += _ptrData[curByte++];

            if(_info.payloadSize[_info.numNALUs] < _info.startCodeSize[_info.numNALUs] + seiPayloadSize)
            {
                // sanity of remaining buffer
                // return 0 since no one "need" SEI messages
                assert(false);
               return 0;
            }

            if(seiPayloadType == 24)
            {
                // we add this to NALU 0 to be signaled in the first PACSI packet
                _info.PACSI[0].numSEINALUs = 1; // we allways add this to NALU 0 to send it in the first packet
                if(_info.PACSI[0].seiMessageLength[0] != seiPayloadSize)
                {
                    _info.PACSI[0].seiMessageLength[0] = seiPayloadSize;
                    delete [] _info.PACSI[0].seiMessageData[0];
                    _info.PACSI[0].seiMessageData[0] = new uint8_t[seiPayloadSize];
                }
                memcpy(_info.PACSI[0].seiMessageData[0], _ptrData+seiStartOffset, seiPayloadSize);

                _info.PACSI[0].NALlength += seiPayloadSize + 2; // additional 2 is the length

#ifdef DEBUG_SEI_MESSAGE
                const uint8_t numberOfLayers = 10;
                uint16_t avgBitrate[numberOfLayers]= {0,0,0,0,0,0,0,0,0,0};
                uint16_t maxBitrateLayer[numberOfLayers]= {0,0,0,0,0,0,0,0,0,0};
                uint16_t maxBitrateLayerRepresentation[numberOfLayers] = {0,0,0,0,0,0,0,0,0,0};
                uint16_t maxBitrareCalcWindow[numberOfLayers] = {0,0,0,0,0,0,0,0,0,0};

                BitstreamParser parserScalabilityInfo(_ptrData+curByte, seiPayloadSize);

                parserScalabilityInfo.Get1Bit(); // not used in futher parsing
                const uint8_t priority_layer_info_present = parserScalabilityInfo.Get1Bit();
                const uint8_t priority_id_setting_flag = parserScalabilityInfo.Get1Bit();

                uint32_t numberOfLayersMinusOne = parserScalabilityInfo.GetUE();
                for(uint32_t j = 0; j<= numberOfLayersMinusOne; j++)
                {
                    printf("\nLayer ID:%d \n",parserScalabilityInfo.GetUE());
                    printf("Priority ID:%d \n", parserScalabilityInfo.Get6Bits());
                    printf("Discardable:%d \n", parserScalabilityInfo.Get1Bit());

                    printf("Dependency ID:%d \n", parserScalabilityInfo.Get3Bits());
                    printf("Quality ID:%d \n", parserScalabilityInfo.Get4Bits());
                    printf("Temporal ID:%d \n", parserScalabilityInfo.Get3Bits());

                    const uint8_t sub_pic_layer_flag = parserScalabilityInfo.Get1Bit();
                    const uint8_t sub_region_layer_flag = parserScalabilityInfo.Get1Bit();
                    const uint8_t iroi_division_info_present_flag = parserScalabilityInfo.Get1Bit();
                    const uint8_t profile_level_info_present_flag = parserScalabilityInfo.Get1Bit();
                    const uint8_t bitrate_info_present_flag = parserScalabilityInfo.Get1Bit();
                    const uint8_t frm_rate_info_present_flag = parserScalabilityInfo.Get1Bit();
                    const uint8_t frm_size_info_present_flag = parserScalabilityInfo.Get1Bit();
                    const uint8_t layer_dependency_info_present_flag = parserScalabilityInfo.Get1Bit();
                    const uint8_t parameter_sets_info_present_flag = parserScalabilityInfo.Get1Bit();
                    const uint8_t bitstream_restriction_info_present_flag = parserScalabilityInfo.Get1Bit();
                    const uint8_t exact_inter_layer_pred_flag = parserScalabilityInfo.Get1Bit();  // not used in futher parsing

                    if(sub_pic_layer_flag || iroi_division_info_present_flag)
                    {
                        parserScalabilityInfo.Get1Bit();
                    }
                    const uint8_t layer_conversion_flag = parserScalabilityInfo.Get1Bit();
                    const uint8_t layer_output_flag = parserScalabilityInfo.Get1Bit();  // not used in futher parsing

                    if(profile_level_info_present_flag)
                    {
                        parserScalabilityInfo.Get24Bits();
                    }
                    if(bitrate_info_present_flag)
                    {
                        // this is what we want
                        avgBitrate[j] = parserScalabilityInfo.Get16Bits();
                        maxBitrateLayer[j] = parserScalabilityInfo.Get16Bits();
                        maxBitrateLayerRepresentation[j] = parserScalabilityInfo.Get16Bits();
                        maxBitrareCalcWindow[j] = parserScalabilityInfo.Get16Bits();

                        printf("\tAvg:%d\n", BitRateBPS(avgBitrate[j]));
                        printf("\tmaxBitrate:%d\n", BitRateBPS(maxBitrateLayer[j]));
                        printf("\tmaxBitrate rep:%d\n", BitRateBPS(maxBitrateLayerRepresentation[j]));
                        printf("\tCalcWindow:%d\n", maxBitrareCalcWindow[j]);
                    }
                    if(frm_rate_info_present_flag)
                    {
                        printf("\tFrame rate constant:%d\n", parserScalabilityInfo.Get2Bits()); // 0 = not constant, 1 = constant, 2 = maybe...
                        printf("\tFrame rate avg:%d\n", parserScalabilityInfo.Get16Bits()/256);
                    }
                    if(frm_size_info_present_flag || iroi_division_info_present_flag)
                    {
                        printf("\tFrame Width:%d\n",(parserScalabilityInfo.GetUE()+1)*16);
                        printf("\tFrame Height:%d\n",(parserScalabilityInfo.GetUE()+1)*16);
                    }
                    if(sub_region_layer_flag)
                    {
                        parserScalabilityInfo.GetUE();
                        if(parserScalabilityInfo.Get1Bit())
                        {
                            parserScalabilityInfo.Get16Bits();
                            parserScalabilityInfo.Get16Bits();
                            parserScalabilityInfo.Get16Bits();
                            parserScalabilityInfo.Get16Bits();
                        }
                    }
                    if(sub_pic_layer_flag)
                    {
                        parserScalabilityInfo.GetUE();
                    }
                    if(iroi_division_info_present_flag)
                    {
                        if(parserScalabilityInfo.Get1Bit())
                        {
                            parserScalabilityInfo.GetUE();
                            parserScalabilityInfo.GetUE();
                        }else
                        {
                            const uint32_t numRoisMinusOne = parserScalabilityInfo.GetUE();
                            for(uint32_t k = 0; k <= numRoisMinusOne; k++)
                            {
                                parserScalabilityInfo.GetUE();
                                parserScalabilityInfo.GetUE();
                                parserScalabilityInfo.GetUE();
                            }
                        }
                    }
                    if(layer_dependency_info_present_flag)
                    {
                        const uint32_t numDirectlyDependentLayers = parserScalabilityInfo.GetUE();
                        for(uint32_t k = 0; k < numDirectlyDependentLayers; k++)
                        {
                            parserScalabilityInfo.GetUE();
                        }
                    } else
                    {
                        parserScalabilityInfo.GetUE();
                    }
                    if(parameter_sets_info_present_flag)
                    {
                        const uint32_t numSeqParameterSetMinusOne = parserScalabilityInfo.GetUE();
                        for(uint32_t k = 0; k <= numSeqParameterSetMinusOne; k++)
                        {
                            parserScalabilityInfo.GetUE();
                        }
                        const uint32_t numSubsetSeqParameterSetMinusOne = parserScalabilityInfo.GetUE();
                        for(uint32_t l = 0; l <= numSubsetSeqParameterSetMinusOne; l++)
                        {
                            parserScalabilityInfo.GetUE();
                        }
                        const uint32_t numPicParameterSetMinusOne = parserScalabilityInfo.GetUE();
                        for(uint32_t m = 0; m <= numPicParameterSetMinusOne; m++)
                        {
                            parserScalabilityInfo.GetUE();
                        }
                    }else
                    {
                        parserScalabilityInfo.GetUE();
                    }
                    if(bitstream_restriction_info_present_flag)
                    {
                        parserScalabilityInfo.Get1Bit();
                        parserScalabilityInfo.GetUE();
                        parserScalabilityInfo.GetUE();
                        parserScalabilityInfo.GetUE();
                        parserScalabilityInfo.GetUE();
                        parserScalabilityInfo.GetUE();
                        parserScalabilityInfo.GetUE();
                    }
                    if(layer_conversion_flag)
                    {
                        parserScalabilityInfo.GetUE();
                        for(uint32_t k = 0; k <2;k++)
                        {
                            if(parserScalabilityInfo.Get1Bit())
                            {
                                parserScalabilityInfo.Get24Bits();
                                parserScalabilityInfo.Get16Bits();
                                parserScalabilityInfo.Get16Bits();
                            }
                        }
                    }
                }
                if(priority_layer_info_present)
                {
                    const uint32_t prNumDidMinusOne = parserScalabilityInfo.GetUE();
                    for(uint32_t k = 0; k <= prNumDidMinusOne;k++)
                    {
                        parserScalabilityInfo.Get3Bits();
                        const uint32_t prNumMinusOne = parserScalabilityInfo.GetUE();
                        for(uint32_t l = 0; l <= prNumMinusOne; l++)
                        {
                            parserScalabilityInfo.GetUE();
                            parserScalabilityInfo.Get24Bits();
                            parserScalabilityInfo.Get16Bits();
                            parserScalabilityInfo.Get16Bits();
                        }
                    }
                }
                if(priority_id_setting_flag)
                {
                    uint8_t priorityIdSettingUri;
                    uint32_t priorityIdSettingUriIdx = 0;
                    do
                    {
                        priorityIdSettingUri = parserScalabilityInfo.Get8Bits();
                    } while (priorityIdSettingUri != 0);
                }
#endif
            } else
            {
                // not seiPayloadType 24 ignore
            }
            //check if we have more SEI in NALU
        } while (_info.payloadSize[_info.numNALUs] > _info.startCodeSize[_info.numNALUs] + seiPayloadSize);
    }

   // Extended NAL unit header (3 bytes).
   // +---------------+---------------+---------------+
   // |0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|
   // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   // |R|I|   PRID    |N| DID |  QID  | TID |U|D|O| RR|
   // +---------------+---------------+---------------+

   // R    - Reserved for future extensions (MUST be 1). Receivers SHOULD ignore the value of R.
   // I    - Is layer representation an IDR layer (1) or not (0).
   // PRID - Priority identifier for the NAL unit.
   // N    - Specifies whether inter-layer prediction may be used for decoding the coded slice (1) or not (0).
   // DID  - Indicates the inter-layer coding dependency level of a layer representation.
   // QID  - Indicates the quality level of an MGS layer representation.
   // TID  - Indicates the temporal level of a layer representation.
   // U    - Use only reference base pictures during the inter prediction process (1) or not (0).
   // D    - Discardable flag.
   // O    - Output_flag. Affects the decoded picture output process as defined in Annex C of [H.264].
   // RR   - Reserved_three_2bits (MUST be '11'). Receivers SHOULD ignore the value of RR.

    if (_info.type[_info.numNALUs] == 14 ||
        _info.type[_info.numNALUs] == 20)
    {
        uint32_t curByte = _info.startCodeSize[_info.numNALUs] + 1;

        if (_remLength < curByte + 3)
        {
                return -1;
        }

        _info.SVCheader[_info.numNALUs].idr        = (_ptrData[curByte] >> 6) & 0x01;
        _info.SVCheader[_info.numNALUs].priorityID = (_ptrData[curByte++] & 0x3F);

        _info.SVCheader[_info.numNALUs].interLayerPred = (_ptrData[curByte] >> 7) & 0x01;
        _info.SVCheader[_info.numNALUs].dependencyID   = (_ptrData[curByte] >> 4) & 0x07;
        _info.SVCheader[_info.numNALUs].qualityID      = (_ptrData[curByte++] & 0x0F);

        _info.SVCheader[_info.numNALUs].temporalID     = (_ptrData[curByte] >> 5) & 0x07;
        _info.SVCheader[_info.numNALUs].useRefBasePic  = (_ptrData[curByte] >> 4) & 0x01;
        _info.SVCheader[_info.numNALUs].discardable    = (_ptrData[curByte] >> 3) & 0x01;
        _info.SVCheader[_info.numNALUs].output         = (_ptrData[curByte] >> 2) & 0x01;

        if (_info.type[_info.numNALUs] == 14)
        {
            // inform the next NALU
            memcpy(&(_info.SVCheader[_info.numNALUs+1]), &(_info.SVCheader[_info.numNALUs]), sizeof(_H264_SVC_NALUHeader));
        }
    }
   return 0;
}


/*******************************************************************************
 * void SetLayerSEBit();
 *
 * Sets start and end bits for the current NAL unit.
 *
 * Output:
 *          - _info.PACSI[currentNALU].S    : First NAL unit in a layer (S = 1).
 *          - _info.PACSI[currentNALU].E    : Last NAL unit in a layer (E = 1).
 *
 */
void
H264Information::SetLayerSEBit(int32_t foundLast)
{
    if (_info.numNALUs == 0)
    {
        // First NAL unit
        _info.PACSI[_info.numNALUs].S = 1;
    }

    if (_info.numNALUs > 0)
    {
        if (_info.type[_info.numNALUs] != _info.type[_info.numNALUs-1] &&
           (_info.type[_info.numNALUs] == 20))
        {
            // First layer in scalable extension
            _info.PACSI[_info.numNALUs].S   = 1;
            _info.PACSI[_info.numNALUs-1].E = 1;
        }

        if (_info.type[_info.numNALUs] == 20 && _info.type[_info.numNALUs-1] == 20)
        {
            if (_info.SVCheader[_info.numNALUs].temporalID   != _info.SVCheader[_info.numNALUs-1].temporalID ||
                _info.SVCheader[_info.numNALUs].dependencyID != _info.SVCheader[_info.numNALUs-1].dependencyID ||
                _info.SVCheader[_info.numNALUs].qualityID    != _info.SVCheader[_info.numNALUs-1].qualityID)
            {
                // New layer in scalable extension
                _info.PACSI[_info.numNALUs].S   = 1;
                _info.PACSI[_info.numNALUs-1].E = 1;
            }
        }
    }

    if (foundLast)
    {
        // Last NAL unit
        _info.PACSI[_info.numNALUs].E = 1;
    }

}

/*******************************************************************************
 * int32_t SetLayerLengths();
 *
 * Sets the accumulated layer length.
 *
 * Output:
 *          - _info.accLayerSize[currentLayer]   : Size in bytes of layer: 0 - currentLayer.
 *
 * Return value:
 *          - 0                        : ok
 *          - (-1)                     : Error
 *
 */
int32_t
H264Information::SetLayerLengths()
{
    for (uint32_t curNALU = 0; curNALU < _info.numNALUs; curNALU++)
    {
        _info.accLayerSize[_info.numLayers] += _info.startCodeSize[curNALU] + _info.payloadSize[curNALU];

        if (_info.PACSI[curNALU].E == 1)
        {
            _info.numLayers++;
            if (curNALU == uint32_t(_info.numNALUs - 1))
            {
                break;
            }
            if (_info.numLayers >= KMaxNumberOfLayers)
            {
                Reset();
                return -1;
            }
            _info.accLayerSize[_info.numLayers] += _info.accLayerSize[_info.numLayers - 1];
        }
    }

    if (_info.numLayers < 1 && _info.numLayers > KMaxNumberOfLayers)
    {
        Reset();
        return -1;
    }

    if (_info.accLayerSize[_info.numLayers - 1] != int32_t(_length))
    {
        Reset();
        return -1;
    }

    return 0;
}
}  // namespace webrtc
