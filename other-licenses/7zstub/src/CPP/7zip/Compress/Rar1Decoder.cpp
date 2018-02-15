// Rar1Decoder.cpp
// According to unRAR license, this code may not be used to develop
// a program that creates RAR archives
 
#include "StdAfx.h"

#include "Rar1Decoder.h"

namespace NCompress {
namespace NRar1 {

static const UInt32 PosL1[] = {0,0,0,2,3,5,7,11,16,20,24,32,32, 256};
static const UInt32 PosL2[] = {0,0,0,0,5,7,9,13,18,22,26,34,36, 256};
static const UInt32 PosHf0[] = {0,0,0,0,0,8,16,24,33,33,33,33,33, 257};
static const UInt32 PosHf1[] = {0,0,0,0,0,0,4,44,60,76,80,80,127, 257};
static const UInt32 PosHf2[] = {0,0,0,0,0,0,2,7,53,117,233, 257,0};
static const UInt32 PosHf3[] = {0,0,0,0,0,0,0,2,16,218,251, 257,0};
static const UInt32 PosHf4[] = {0,0,0,0,0,0,0,0,0,255, 257,0,0};

static const UInt32 kHistorySize = (1 << 16);

/*
class CCoderReleaser
{
  CDecoder *m_Coder;
public:
  CCoderReleaser(CDecoder *coder): m_Coder(coder) {}
  ~CCoderReleaser() { m_Coder->ReleaseStreams(); }
};
*/

CDecoder::CDecoder(): m_IsSolid(false), _errorMode(false) { }

void CDecoder::InitStructures()
{
  for (int i = 0; i < kNumRepDists; i++)
    m_RepDists[i] = 0;
  m_RepDistPtr = 0;
  LastLength = 0;
  LastDist = 0;
}

UInt32 CDecoder::ReadBits(int numBits) { return m_InBitStream.ReadBits(numBits); }

HRESULT CDecoder::CopyBlock(UInt32 distance, UInt32 len)
{
  if (len == 0)
    return S_FALSE;
  m_UnpackSize -= len;
  return m_OutWindowStream.CopyBlock(distance, len) ? S_OK : S_FALSE;
}


UInt32 CDecoder::DecodeNum(const UInt32 *posTab)
{
  UInt32 startPos = 2;
  UInt32 num = m_InBitStream.GetValue(12);
  for (;;)
  {
    UInt32 cur = (posTab[(size_t)startPos + 1] - posTab[startPos]) << (12 - startPos);
    if (num < cur)
      break;
    startPos++;
    num -= cur;
  }
  m_InBitStream.MovePos(startPos);
  return((num >> (12 - startPos)) + posTab[startPos]);
}

static const Byte kShortLen1 [] = {1,3,4,4,5,6,7,8,8,4,4,5,6,6 };
static const Byte kShortLen1a[] = {1,4,4,4,5,6,7,8,8,4,4,5,6,6,4 };
static const Byte kShortLen2 [] = {2,3,3,3,4,4,5,6,6,4,4,5,6,6 };
static const Byte kShortLen2a[] = {2,3,3,4,4,4,5,6,6,4,4,5,6,6,4 };
static const UInt32 kShortXor1[] = {0,0xa0,0xd0,0xe0,0xf0,0xf8,0xfc,0xfe,0xff,0xc0,0x80,0x90,0x98,0x9c,0xb0};
static const UInt32 kShortXor2[] = {0,0x40,0x60,0xa0,0xd0,0xe0,0xf0,0xf8,0xfc,0xc0,0x80,0x90,0x98,0x9c,0xb0};

HRESULT CDecoder::ShortLZ()
{
  UInt32 len, saveLen, dist;
  int distancePlace;
  const Byte *kShortLen;
  const UInt32 *kShortXor;
  NumHuf = 0;

  if (LCount == 2)
  {
    if (ReadBits(1))
      return CopyBlock(LastDist, LastLength);
    LCount = 0;
  }

  UInt32 bitField = m_InBitStream.GetValue(8);

  if (AvrLn1 < 37)
  {
    kShortLen = Buf60 ? kShortLen1a : kShortLen1;
    kShortXor = kShortXor1;
  }
  else
  {
    kShortLen = Buf60 ? kShortLen2a : kShortLen2;
    kShortXor = kShortXor2;
  }

  for (len = 0; ((bitField ^ kShortXor[len]) & (~(0xff >> kShortLen[len]))) != 0; len++);
  m_InBitStream.MovePos(kShortLen[len]);

  if (len >= 9)
  {
    if (len == 9)
    {
      LCount++;
      return CopyBlock(LastDist, LastLength);
    }
    if (len == 14)
    {
      LCount = 0;
      len = DecodeNum(PosL2) + 5;
      dist = 0x8000 + ReadBits(15) - 1;
      LastLength = len;
      LastDist = dist;
      return CopyBlock(dist, len);
    }

    LCount = 0;
    saveLen = len;
    dist = m_RepDists[(m_RepDistPtr - (len - 9)) & 3];
    len = DecodeNum(PosL1) + 2;
    if (len == 0x101 && saveLen == 10)
    {
      Buf60 ^= 1;
      return S_OK;
    }
    if (dist >= 256)
      len++;
    if (dist >= MaxDist3 - 1)
      len++;
  }
  else
  {
    LCount = 0;
    AvrLn1 += len;
    AvrLn1 -= AvrLn1 >> 4;
    
    distancePlace = DecodeNum(PosHf2) & 0xff;
    dist = ChSetA[(unsigned)distancePlace];
    if (--distancePlace != -1)
    {
      PlaceA[dist]--;
      UInt32 lastDistance = ChSetA[(unsigned)distancePlace];
      PlaceA[lastDistance]++;
      ChSetA[(size_t)(unsigned)distancePlace + 1] = lastDistance;
      ChSetA[(unsigned)distancePlace] = dist;
    }
    len += 2;
  }

  m_RepDists[m_RepDistPtr++] = dist;
  m_RepDistPtr &= 3;
  LastLength = len;
  LastDist = dist;
  return CopyBlock(dist, len);
}


HRESULT CDecoder::LongLZ()
{
  UInt32 len;
  UInt32 dist;
  UInt32 distancePlace, newDistancePlace;
  UInt32 oldAvr2, oldAvr3;

  NumHuf = 0;
  Nlzb += 16;
  if (Nlzb > 0xff)
  {
    Nlzb = 0x90;
    Nhfb >>= 1;
  }
  oldAvr2=AvrLn2;

  if (AvrLn2 >= 122)
    len = DecodeNum(PosL2);
  else if (AvrLn2 >= 64)
    len = DecodeNum(PosL1);
  else
  {
    UInt32 bitField = m_InBitStream.GetValue(16);
    if (bitField < 0x100)
    {
      len = bitField;
      m_InBitStream.MovePos(16);
    }
    else
    {
      for (len = 0; ((bitField << len) & 0x8000) == 0; len++)
        ;
      m_InBitStream.MovePos(len + 1);
    }
  }

  AvrLn2 += len;
  AvrLn2 -= AvrLn2 >> 5;

  if (AvrPlcB > 0x28ff)
    distancePlace = DecodeNum(PosHf2);
  else if (AvrPlcB > 0x6ff)
    distancePlace = DecodeNum(PosHf1);
  else
    distancePlace = DecodeNum(PosHf0);

  AvrPlcB += distancePlace;
  AvrPlcB -= AvrPlcB >> 8;
  
  for (;;)
  {
    dist = ChSetB[distancePlace & 0xff];
    newDistancePlace = NToPlB[dist++ & 0xff]++;
    if (!(dist & 0xff))
      CorrHuff(ChSetB,NToPlB);
    else
      break;
  }

  ChSetB[distancePlace] = ChSetB[newDistancePlace];
  ChSetB[newDistancePlace] = dist;

  dist = ((dist & 0xff00) >> 1) | ReadBits(7);

  oldAvr3 = AvrLn3;
  
  if (len != 1 && len != 4)
    if (len == 0 && dist <= MaxDist3)
    {
      AvrLn3++;
      AvrLn3 -= AvrLn3 >> 8;
    }
    else
      if (AvrLn3 > 0)
        AvrLn3--;
  
  len += 3;
  
  if (dist >= MaxDist3)
    len++;
  if (dist <= 256)
    len += 8;
  
  if (oldAvr3 > 0xb0 || AvrPlc >= 0x2a00 && oldAvr2 < 0x40)
    MaxDist3 = 0x7f00;
  else
    MaxDist3 = 0x2001;
  
  m_RepDists[m_RepDistPtr++] = --dist;
  m_RepDistPtr &= 3;
  LastLength = len;
  LastDist = dist;
  
  return CopyBlock(dist, len);
}


HRESULT CDecoder::HuffDecode()
{
  UInt32 curByte, newBytePlace;
  UInt32 len;
  UInt32 dist;
  int bytePlace;

  if      (AvrPlc > 0x75ff)  bytePlace = DecodeNum(PosHf4);
  else if (AvrPlc > 0x5dff)  bytePlace = DecodeNum(PosHf3);
  else if (AvrPlc > 0x35ff)  bytePlace = DecodeNum(PosHf2);
  else if (AvrPlc > 0x0dff)  bytePlace = DecodeNum(PosHf1);
  else                       bytePlace = DecodeNum(PosHf0);
  
  if (StMode)
  {
    if (--bytePlace == -1)
    {
      if (ReadBits(1))
      {
        NumHuf = StMode = 0;
        return S_OK;
      }
      else
      {
        len = (ReadBits(1)) ? 4 : 3;
        dist = DecodeNum(PosHf2);
        dist = (dist << 5) | ReadBits(5);
        return CopyBlock(dist - 1, len);
      }
    }
  }
  else if (NumHuf++ >= 16 && FlagsCnt == 0)
    StMode = 1;
  
  bytePlace &= 0xff;
  AvrPlc += bytePlace;
  AvrPlc -= AvrPlc >> 8;
  Nhfb+=16;
  
  if (Nhfb > 0xff)
  {
    Nhfb=0x90;
    Nlzb >>= 1;
  }

  m_UnpackSize --;
  m_OutWindowStream.PutByte((Byte)(ChSet[bytePlace] >> 8));

  for (;;)
  {
    curByte = ChSet[bytePlace];
    newBytePlace = NToPl[curByte++ & 0xff]++;
    if ((curByte & 0xff) > 0xa1)
      CorrHuff(ChSet, NToPl);
    else
      break;
  }

  ChSet[bytePlace] = ChSet[newBytePlace];
  ChSet[newBytePlace] = curByte;
  return S_OK;
}


void CDecoder::GetFlagsBuf()
{
  UInt32 flags, newFlagsPlace;
  UInt32 flagsPlace = DecodeNum(PosHf2);

  for (;;)
  {
    flags = ChSetC[flagsPlace];
    FlagBuf = flags >> 8;
    newFlagsPlace = NToPlC[flags++ & 0xff]++;
    if ((flags & 0xff) != 0)
      break;
    CorrHuff(ChSetC, NToPlC);
  }

  ChSetC[flagsPlace] = ChSetC[newFlagsPlace];
  ChSetC[newFlagsPlace] = flags;
}

void CDecoder::InitData()
{
  if (!m_IsSolid)
  {
    AvrPlcB = AvrLn1 = AvrLn2 = AvrLn3 = NumHuf = Buf60 = 0;
    AvrPlc = 0x3500;
    MaxDist3 = 0x2001;
    Nhfb = Nlzb = 0x80;
  }
  FlagsCnt = 0;
  FlagBuf = 0;
  StMode = 0;
  LCount = 0;
}

void CDecoder::CorrHuff(UInt32 *CharSet,UInt32 *NumToPlace)
{
  int i;
  for (i = 7; i >= 0; i--)
    for (int j = 0; j < 32; j++, CharSet++)
      *CharSet = (*CharSet & ~0xff) | i;
  memset(NumToPlace, 0, sizeof(NToPl));
  for (i = 6; i >= 0; i--)
    NumToPlace[i] = (7 - i) * 32;
}

void CDecoder::InitHuff()
{
  for (UInt32 i = 0; i < 256; i++)
  {
    Place[i] = PlaceA[i] = PlaceB[i] = i;
    PlaceC[i] = (~i + 1) & 0xff;
    ChSet[i] = ChSetB[i] = i << 8;
    ChSetA[i] = i;
    ChSetC[i] = ((~i + 1) & 0xff) << 8;
  }
  memset(NToPl, 0, sizeof(NToPl));
  memset(NToPlB, 0, sizeof(NToPlB));
  memset(NToPlC, 0, sizeof(NToPlC));
  CorrHuff(ChSetB, NToPlB);
}

HRESULT CDecoder::CodeReal(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo * /* progress */)
{
  if (inSize == NULL || outSize == NULL)
    return E_INVALIDARG;

  if (!m_OutWindowStream.Create(kHistorySize))
    return E_OUTOFMEMORY;
  if (!m_InBitStream.Create(1 << 20))
    return E_OUTOFMEMORY;

  m_UnpackSize = (Int64)*outSize;
  m_OutWindowStream.SetStream(outStream);
  m_OutWindowStream.Init(m_IsSolid);
  m_InBitStream.SetStream(inStream);
  m_InBitStream.Init();

  // CCoderReleaser coderReleaser(this);
  InitData();
  if (!m_IsSolid)
  {
    _errorMode = false;
    InitStructures();
    InitHuff();
  }
  
  if (_errorMode)
    return S_FALSE;
    
  if (m_UnpackSize > 0)
  {
    GetFlagsBuf();
    FlagsCnt = 8;
  }

  while (m_UnpackSize > 0)
  {
    if (StMode)
    {
      RINOK(HuffDecode());
      continue;
    }

    if (--FlagsCnt < 0)
    {
      GetFlagsBuf();
      FlagsCnt=7;
    }

    if (FlagBuf & 0x80)
    {
      FlagBuf <<= 1;
      if (Nlzb > Nhfb)
      {
        RINOK(LongLZ());
      }
      else
      {
        RINOK(HuffDecode());
      }
    }
    else
    {
      FlagBuf <<= 1;
      if (--FlagsCnt < 0)
      {
        GetFlagsBuf();
        FlagsCnt = 7;
      }
      if (FlagBuf & 0x80)
      {
        FlagBuf <<= 1;
        if (Nlzb > Nhfb)
        {
          RINOK(HuffDecode());
        }
        else
        {
          RINOK(LongLZ());
        }
      }
      else
      {
        FlagBuf <<= 1;
        RINOK(ShortLZ());
      }
    }
  }
  if (m_UnpackSize < 0)
    return S_FALSE;
  return m_OutWindowStream.Flush();
}

STDMETHODIMP CDecoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress)
{
  try { return CodeReal(inStream, outStream, inSize, outSize, progress); }
  catch(const CInBufferException &e) { _errorMode = true; return e.ErrorCode; }
  catch(const CLzOutWindowException &e) { _errorMode = true; return e.ErrorCode; }
  catch(...) { _errorMode = true; return S_FALSE; }
}

STDMETHODIMP CDecoder::SetDecoderProperties2(const Byte *data, UInt32 size)
{
  if (size < 1)
    return E_INVALIDARG;
  m_IsSolid = ((data[0] & 1) != 0);
  return S_OK;
}

}}
