/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef NETEQTEST_CODECCLASS_H
#define NETEQTEST_CODECCLASS_H

#include <string>
#include <string.h>

#include "typedefs.h"
#include "webrtc_neteq.h"
#include "NETEQTEST_NetEQClass.h"

class NETEQTEST_Decoder
{
public:
    NETEQTEST_Decoder(enum WebRtcNetEQDecoder type, WebRtc_UWord16 fs, const char * name, WebRtc_UWord8 pt = 0);
    virtual ~NETEQTEST_Decoder() {};

    virtual int loadToNetEQ(NETEQTEST_NetEQClass & neteq) = 0;

    int getName(char * name, int maxLen) const { strncpy( name, _name.c_str(), maxLen ); return 0;};

    void setPT(WebRtc_UWord8 pt) { _pt = pt; };
    WebRtc_UWord16 getFs() const { return (_fs); };
    enum WebRtcNetEQDecoder getType() const { return (_decoderType); };
    WebRtc_UWord8 getPT() const { return (_pt); };

protected:
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq, WebRtcNetEQ_CodecDef & codecInst);

    void * _decoder;
    enum WebRtcNetEQDecoder _decoderType;
    WebRtc_UWord8 _pt;
    WebRtc_UWord16 _fs;
    std::string _name;

private:
};


class decoder_iSAC : public NETEQTEST_Decoder
{
public:
    decoder_iSAC(WebRtc_UWord8 pt = 0);
    virtual ~decoder_iSAC();
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};


class decoder_iSACSWB : public NETEQTEST_Decoder
{
public:
    decoder_iSACSWB(WebRtc_UWord8 pt = 0);
    virtual ~decoder_iSACSWB();
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};


class decoder_iSACFB : public NETEQTEST_Decoder {
 public:
  decoder_iSACFB(WebRtc_UWord8 pt = 0);
  virtual ~decoder_iSACFB();
  int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};


class decoder_PCMU : public NETEQTEST_Decoder
{
public:
    decoder_PCMU(WebRtc_UWord8 pt = 0);
    virtual ~decoder_PCMU() {};
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};


class decoder_PCMA : public NETEQTEST_Decoder
{
public:
    decoder_PCMA(WebRtc_UWord8 pt = 0);
    virtual ~decoder_PCMA() {};
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};

class decoder_PCM16B_NB : public NETEQTEST_Decoder
{
public:
    decoder_PCM16B_NB(WebRtc_UWord8 pt = 0) : NETEQTEST_Decoder(kDecoderPCM16B, 8000, "PCM16 nb", pt) {};
    virtual ~decoder_PCM16B_NB() {};
    virtual int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};
class decoder_PCM16B_WB : public NETEQTEST_Decoder
{
public:
    decoder_PCM16B_WB(WebRtc_UWord8 pt = 0) : NETEQTEST_Decoder(kDecoderPCM16Bwb, 16000, "PCM16 wb", pt) {};
    virtual ~decoder_PCM16B_WB() {};
    virtual int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};
class decoder_PCM16B_SWB32 : public NETEQTEST_Decoder
{
public:
    decoder_PCM16B_SWB32(WebRtc_UWord8 pt = 0) : NETEQTEST_Decoder(kDecoderPCM16Bswb32kHz, 32000, "PCM16 swb32", pt) {};
    virtual ~decoder_PCM16B_SWB32() {};
    virtual int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};
class decoder_PCM16B_SWB48 : public NETEQTEST_Decoder
{
public:
    decoder_PCM16B_SWB48(WebRtc_UWord8 pt = 0) : NETEQTEST_Decoder(kDecoderPCM16Bswb48kHz, 48000, "PCM16 swb48", pt) {};
    virtual ~decoder_PCM16B_SWB48() {};
    virtual int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};


class decoder_ILBC : public NETEQTEST_Decoder
{
public:
    decoder_ILBC(WebRtc_UWord8 pt = 0);
    virtual ~decoder_ILBC();
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};


class decoder_G729 : public NETEQTEST_Decoder
{
public:
    decoder_G729(WebRtc_UWord8 pt = 0);
    virtual ~decoder_G729();
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};

class decoder_G729_1 : public NETEQTEST_Decoder
{
public:
    decoder_G729_1(WebRtc_UWord8 pt = 0);
    virtual ~decoder_G729_1();
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};


class decoder_G722 : public NETEQTEST_Decoder
{
public:
    decoder_G722(WebRtc_UWord8 pt = 0);
    virtual ~decoder_G722();
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};


class decoder_G722_1_16 : public NETEQTEST_Decoder
{
public:
    decoder_G722_1_16(WebRtc_UWord8 pt = 0);
    virtual ~decoder_G722_1_16();
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};

class decoder_G722_1_24 : public NETEQTEST_Decoder
{
public:
    decoder_G722_1_24(WebRtc_UWord8 pt = 0);
    virtual ~decoder_G722_1_24();
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};

class decoder_G722_1_32 : public NETEQTEST_Decoder
{
public:
    decoder_G722_1_32(WebRtc_UWord8 pt = 0);
    virtual ~decoder_G722_1_32();
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};


class decoder_G722_1C_24 : public NETEQTEST_Decoder
{
public:
    decoder_G722_1C_24(WebRtc_UWord8 pt = 0);
    virtual ~decoder_G722_1C_24();
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};

class decoder_G722_1C_32 : public NETEQTEST_Decoder
{
public:
    decoder_G722_1C_32(WebRtc_UWord8 pt = 0);
    virtual ~decoder_G722_1C_32();
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};

class decoder_G722_1C_48 : public NETEQTEST_Decoder
{
public:
    decoder_G722_1C_48(WebRtc_UWord8 pt = 0);
    virtual ~decoder_G722_1C_48();
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};


class decoder_AMR : public NETEQTEST_Decoder
{
public:
    decoder_AMR(WebRtc_UWord8 pt = 0);
    virtual ~decoder_AMR();
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};

class decoder_AMRWB : public NETEQTEST_Decoder
{
public:
    decoder_AMRWB(WebRtc_UWord8 pt = 0);
    virtual ~decoder_AMRWB();
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};

class decoder_GSMFR : public NETEQTEST_Decoder
{
public:
    decoder_GSMFR(WebRtc_UWord8 pt = 0);
    virtual ~decoder_GSMFR();
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};

class decoder_G726 : public NETEQTEST_Decoder
{
public:
    //virtual decoder_G726(WebRtc_UWord8 pt = 0) = 0;
    decoder_G726(enum WebRtcNetEQDecoder type, const char * name, WebRtc_UWord8 pt = 0);
    virtual ~decoder_G726();
    virtual int loadToNetEQ(NETEQTEST_NetEQClass & neteq) = 0;
};

class decoder_G726_16 : public decoder_G726
{
public:
    decoder_G726_16(WebRtc_UWord8 pt = 0) : decoder_G726(kDecoderG726_16, "G.726 (16 kbps)", pt) {};
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};

class decoder_G726_24 : public decoder_G726
{
public:
    decoder_G726_24(WebRtc_UWord8 pt = 0) : decoder_G726(kDecoderG726_24, "G.726 (24 kbps)", pt) {};
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};

class decoder_G726_32 : public decoder_G726
{
public:
    decoder_G726_32(WebRtc_UWord8 pt = 0) : decoder_G726(kDecoderG726_32, "G.726 (32 kbps)", pt) {};
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};

class decoder_G726_40 : public decoder_G726
{
public:
    decoder_G726_40(WebRtc_UWord8 pt = 0) : decoder_G726(kDecoderG726_40, "G.726 (40 kbps)", pt) {};
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};

class decoder_SPEEX : public NETEQTEST_Decoder
{
public:
    decoder_SPEEX(WebRtc_UWord8 pt = 0, WebRtc_UWord16 fs = 8000);
    virtual ~decoder_SPEEX();
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};

class decoder_CELT : public NETEQTEST_Decoder
{
public:
    decoder_CELT(WebRtc_UWord8 pt = 0, WebRtc_UWord16 fs = 32000);
    virtual ~decoder_CELT();
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};
class decoder_CELTslave : public NETEQTEST_Decoder
{
public:
    decoder_CELTslave(WebRtc_UWord8 pt = 0, WebRtc_UWord16 fs = 32000);
    virtual ~decoder_CELTslave();
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};

class decoder_RED : public NETEQTEST_Decoder
{
public:
    decoder_RED(WebRtc_UWord8 pt = 0) : NETEQTEST_Decoder(kDecoderRED, 8000, "RED", pt) {};
    virtual ~decoder_RED() {};
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};

class decoder_AVT : public NETEQTEST_Decoder
{
public:
    decoder_AVT(WebRtc_UWord8 pt = 0) : NETEQTEST_Decoder(kDecoderAVT, 8000, "AVT", pt) {};
    virtual ~decoder_AVT() {};
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};


class decoder_CNG : public NETEQTEST_Decoder
{
public:
    decoder_CNG(WebRtc_UWord8 pt = 0, WebRtc_UWord16 fs = 8000);
    virtual ~decoder_CNG();
    int loadToNetEQ(NETEQTEST_NetEQClass & neteq);
};

#endif //NETEQTEST_CODECCLASS_H
