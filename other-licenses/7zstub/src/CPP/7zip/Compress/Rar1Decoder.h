// Rar1Decoder.h
// According to unRAR license, this code may not be used to develop
// a program that creates RAR archives

#ifndef __COMPRESS_RAR1_DECODER_H
#define __COMPRESS_RAR1_DECODER_H

#include "../../Common/MyCom.h"

#include "../ICoder.h"

#include "../Common/InBuffer.h"

#include "BitmDecoder.h"
#include "HuffmanDecoder.h"
#include "LzOutWindow.h"

namespace NCompress {
namespace NRar1 {

const UInt32 kNumRepDists = 4;

typedef NBitm::CDecoder<CInBuffer> CBitDecoder;

class CDecoder :
  public ICompressCoder,
  public ICompressSetDecoderProperties2,
  public CMyUnknownImp
{
public:
  CLzOutWindow m_OutWindowStream;
  CBitDecoder m_InBitStream;

  UInt32 m_RepDists[kNumRepDists];
  UInt32 m_RepDistPtr;

  UInt32 LastDist;
  UInt32 LastLength;

  Int64 m_UnpackSize;
  bool m_IsSolid;
  bool _errorMode;

  UInt32 ReadBits(int numBits);
  HRESULT CopyBlock(UInt32 distance, UInt32 len);

  UInt32 DecodeNum(const UInt32 *posTab);
  HRESULT ShortLZ();
  HRESULT LongLZ();
  HRESULT HuffDecode();
  void GetFlagsBuf();
  void InitData();
  void InitHuff();
  void CorrHuff(UInt32 *CharSet, UInt32 *NumToPlace);
  void OldUnpWriteBuf();
  
  UInt32 ChSet[256],ChSetA[256],ChSetB[256],ChSetC[256];
  UInt32 Place[256],PlaceA[256],PlaceB[256],PlaceC[256];
  UInt32 NToPl[256],NToPlB[256],NToPlC[256];
  UInt32 FlagBuf,AvrPlc,AvrPlcB,AvrLn1,AvrLn2,AvrLn3;
  int Buf60,NumHuf,StMode,LCount,FlagsCnt;
  UInt32 Nhfb,Nlzb,MaxDist3;

  void InitStructures();

  HRESULT CodeReal(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);

public:
  CDecoder();

  MY_UNKNOWN_IMP1(ICompressSetDecoderProperties2)

  /*
  void ReleaseStreams()
  {
    m_OutWindowStream.ReleaseStream();
    m_InBitStream.ReleaseStream();
  }
  */

  STDMETHOD(Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);

  STDMETHOD(SetDecoderProperties2)(const Byte *data, UInt32 size);

};

}}

#endif
