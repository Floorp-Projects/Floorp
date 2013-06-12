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
    const WebRtc_UWord8 r;
    WebRtc_UWord8       idr;
    WebRtc_UWord8       priorityID;
    WebRtc_UWord8       interLayerPred;
    WebRtc_UWord8       dependencyID;
    WebRtc_UWord8       qualityID;
    WebRtc_UWord8       temporalID;
    WebRtc_UWord8       useRefBasePic;
    WebRtc_UWord8       discardable;
    WebRtc_UWord8       output;
    const WebRtc_UWord8 rr;
    const WebRtc_UWord8 length;
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

    WebRtc_UWord32        NALlength;
    const WebRtc_UWord8   type;
    WebRtc_UWord8         X;
    WebRtc_UWord8         Y;
//  WebRtc_UWord8         T;
    WebRtc_UWord8         A;
    WebRtc_UWord8         P;
    WebRtc_UWord8         C;
    WebRtc_UWord8         S;
    WebRtc_UWord8         E;
    WebRtc_UWord8         TL0picIDx;
    WebRtc_UWord16        IDRpicID;
    WebRtc_UWord16        DONC;
    WebRtc_UWord32        numSEINALUs;
    WebRtc_UWord32        seiMessageLength[KMaxNumberOfSEINALUs]; // we allow KMaxNumberOfSEINALUs SEI messages
    WebRtc_UWord8*        seiMessageData[KMaxNumberOfSEINALUs];
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
    WebRtc_UWord16             numNALUs;
    WebRtc_UWord8              numLayers;
    WebRtc_UWord8              startCodeSize[KMaxNumberOfNALUs];
    WebRtc_UWord32             payloadSize[KMaxNumberOfNALUs];
    WebRtc_UWord8              NRI[KMaxNumberOfNALUs];
    WebRtc_UWord8              type[KMaxNumberOfNALUs];
    H264_SVC_NALUHeader SVCheader[KMaxNumberOfNALUs];
    H264_PACSI_NALU     PACSI[KMaxNumberOfNALUs];
    WebRtc_Word32              accLayerSize[KMaxNumberOfLayers];
};


class H264Information : public VideoCodecInformation
{
public:
    H264Information(const bool SVC);
    ~H264Information();

    virtual void Reset();

    virtual RtpVideoCodecTypes Type();

    virtual WebRtc_Word32 GetInfo(const WebRtc_UWord8* ptrEncodedBuffer, const WebRtc_UWord32 length, const H264Info*& ptrInfo);


protected:
    bool HasInfo(const WebRtc_UWord32 length);
    WebRtc_Word32  FindInfo(const WebRtc_UWord8* ptrEncodedBuffer, const WebRtc_UWord32 length);

    void GetNRI();
    WebRtc_Word32 FindNALU();
    WebRtc_Word32 FindNALUStartCodeSize();
    WebRtc_Word32 FindNALUType();

    WebRtc_Word32 ParseSVCNALUHeader();

    void SetLayerSEBit(WebRtc_Word32 foundLast);
    WebRtc_Word32 SetLayerLengths();

private:
    const bool            _SVC;
    const WebRtc_UWord8*    _ptrData;
    WebRtc_UWord32          _length;
    WebRtc_UWord32          _parsedLength;
    WebRtc_UWord32          _remLength;
    H264Info          _info;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_H264_H264_INFORMATION_H_
