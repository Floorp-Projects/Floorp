/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_H264_H264_INFORMATION_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_H264_H264_INFORMATION_H_

#include "VideoCodecInformation.h"
#include "typedefs.h"

namespace webrtc {
enum
{
    KMaxNumberOfNALUs = 128,
    KMaxNumberOfSEINALUs = 2,
    KMaxNumberOfLayers = 16
};

struct H264_SVC_NALUHeader
{
    H264_SVC_NALUHeader()
    :
    r(1),
    idr(0),
    priorityID(0),
    interLayerPred(0),
    dependencyID(0),
    qualityID(0),
    temporalID(0),
    useRefBasePic(0),
    discardable(0),
    output(0),
    rr(3),
    length(3)
    {
    }
    const uint8_t r;
    uint8_t       idr;
    uint8_t       priorityID;
    uint8_t       interLayerPred;
    uint8_t       dependencyID;
    uint8_t       qualityID;
    uint8_t       temporalID;
    uint8_t       useRefBasePic;
    uint8_t       discardable;
    uint8_t       output;
    const uint8_t rr;
    const uint8_t length;
};

class H264_PACSI_NALU
{
public:
    H264_PACSI_NALU() :
        NALlength(5),
        type(30),
        X(0),
        Y(0),
//        T(0),
        A(0),
        P(0),
        C(0),
        S(0),
        E(0),
        TL0picIDx(0),
        IDRpicID(0),
        DONC(0),
        numSEINALUs(0)
    {
        memset(seiMessageLength, 0, sizeof(seiMessageLength));
        memset(seiMessageData, 0, sizeof(seiMessageData));
    }
    ~H264_PACSI_NALU()
    {
        for(int i = 0; i<KMaxNumberOfSEINALUs; i++)
        {
            if(seiMessageData[i])
            {
                delete [] seiMessageData[i];
            }
        }
    }

    uint32_t        NALlength;
    const uint8_t   type;
    uint8_t         X;
    uint8_t         Y;
//  uint8_t         T;
    uint8_t         A;
    uint8_t         P;
    uint8_t         C;
    uint8_t         S;
    uint8_t         E;
    uint8_t         TL0picIDx;
    uint16_t        IDRpicID;
    uint16_t        DONC;
    uint32_t        numSEINALUs;
    uint32_t        seiMessageLength[KMaxNumberOfSEINALUs]; // we allow KMaxNumberOfSEINALUs SEI messages
    uint8_t*        seiMessageData[KMaxNumberOfSEINALUs];
};

struct H264Info
{
    H264Info()
        :
        numNALUs(0),
        numLayers(0)
        {
            memset(startCodeSize, 0, sizeof(startCodeSize));
            memset(payloadSize, 0, sizeof(payloadSize));
            memset(NRI, 0, sizeof(NRI));
            memset(type, 0, sizeof(type));
            memset(accLayerSize, 0, sizeof(accLayerSize));
        }
    uint16_t             numNALUs;
    uint8_t              numLayers;
    uint8_t              startCodeSize[KMaxNumberOfNALUs];
    uint32_t             payloadSize[KMaxNumberOfNALUs];
    uint8_t              NRI[KMaxNumberOfNALUs];
    uint8_t              type[KMaxNumberOfNALUs];
    H264_SVC_NALUHeader SVCheader[KMaxNumberOfNALUs];
    H264_PACSI_NALU     PACSI[KMaxNumberOfNALUs];
    int32_t              accLayerSize[KMaxNumberOfLayers];
};


class H264Information : public VideoCodecInformation
{
public:
    H264Information(const bool SVC);
    ~H264Information();

    virtual void Reset();

    virtual RtpVideoCodecTypes Type();

    virtual int32_t GetInfo(const uint8_t* ptrEncodedBuffer, const uint32_t length, const H264Info*& ptrInfo);


protected:
    bool HasInfo(const uint32_t length);
    int32_t  FindInfo(const uint8_t* ptrEncodedBuffer, const uint32_t length);

    void GetNRI();
    int32_t FindNALU();
    int32_t FindNALUStartCodeSize();
    int32_t FindNALUType();

    int32_t ParseSVCNALUHeader();

    void SetLayerSEBit(int32_t foundLast);
    int32_t SetLayerLengths();

private:
    const bool            _SVC;
    const uint8_t*    _ptrData;
    uint32_t          _length;
    uint32_t          _parsedLength;
    uint32_t          _remLength;
    H264Info          _info;
};
}  // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_H264_H264_INFORMATION_H_
