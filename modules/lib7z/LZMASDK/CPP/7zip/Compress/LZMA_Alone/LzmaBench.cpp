// LzmaBench.cpp

#include "StdAfx.h"

#include "LzmaBench.h"

#ifndef _WIN32
#define USE_POSIX_TIME
#define USE_POSIX_TIME2
#endif

#ifdef USE_POSIX_TIME
#include <time.h>
#ifdef USE_POSIX_TIME2
#include <sys/time.h>
#endif
#endif

#ifdef _WIN32
#define USE_ALLOCA
#endif

#ifdef USE_ALLOCA
#ifdef _WIN32
#include <malloc.h>
#else
#include <stdlib.h>
#endif
#endif

#include "../../../../C/7zCrc.h"
#include "../../../../C/Alloc.h"

#include "../../../Common/MyCom.h"

#ifdef BENCH_MT
#include "../../../Windows/Synchronization.h"
#include "../../../Windows/Thread.h"
#endif

#ifdef EXTERNAL_LZMA
#include "../../../Windows/PropVariant.h"
#include "../../ICoder.h"
#else
#include "../LzmaDecoder.h"
#include "../LzmaEncoder.h"
#endif

static const UInt32 kUncompressMinBlockSize =
#ifdef UNDER_CE
1 << 24;
#else
1 << 26;
#endif

static const UInt32 kCrcBlockSize =
#ifdef UNDER_CE
1 << 25;
#else
1 << 30;
#endif

static const UInt32 kAdditionalSize = (1 << 16);
static const UInt32 kCompressedAdditionalSize = (1 << 10);
static const UInt32 kMaxLzmaPropSize = 5;

class CBaseRandomGenerator
{
  UInt32 A1;
  UInt32 A2;
public:
  CBaseRandomGenerator() { Init(); }
  void Init() { A1 = 362436069; A2 = 521288629;}
  UInt32 GetRnd()
  {
    return
      ((A1 = 36969 * (A1 & 0xffff) + (A1 >> 16)) << 16) +
      ((A2 = 18000 * (A2 & 0xffff) + (A2 >> 16)) );
  }
};

class CBenchBuffer
{
public:
  size_t BufferSize;
  Byte *Buffer;
  CBenchBuffer(): Buffer(0) {}
  virtual ~CBenchBuffer() { Free(); }
  void Free()
  {
    ::MidFree(Buffer);
    Buffer = 0;
  }
  bool Alloc(size_t bufferSize)
  {
    if (Buffer != 0 && BufferSize == bufferSize)
      return true;
    Free();
    Buffer = (Byte *)::MidAlloc(bufferSize);
    BufferSize = bufferSize;
    return (Buffer != 0);
  }
};

class CBenchRandomGenerator: public CBenchBuffer
{
  CBaseRandomGenerator *RG;
public:
  void Set(CBaseRandomGenerator *rg) { RG = rg; }
  UInt32 GetVal(UInt32 &res, int numBits)
  {
    UInt32 val = res & (((UInt32)1 << numBits) - 1);
    res >>= numBits;
    return val;
  }
  UInt32 GetLen(UInt32 &res)
  {
    UInt32 len = GetVal(res, 2);
    return GetVal(res, 1 + len);
  }
  void Generate()
  {
    UInt32 pos = 0;
    UInt32 rep0 = 1;
    while (pos < BufferSize)
    {
      UInt32 res = RG->GetRnd();
      res >>= 1;
      if (GetVal(res, 1) == 0 || pos < 1024)
        Buffer[pos++] = (Byte)(res & 0xFF);
      else
      {
        UInt32 len;
        len = 1 + GetLen(res);
        if (GetVal(res, 3) != 0)
        {
          len += GetLen(res);
          do
          {
            UInt32 ppp = GetVal(res, 5) + 6;
            res = RG->GetRnd();
            if (ppp > 30)
              continue;
            rep0 = /* (1 << ppp) +*/  GetVal(res, ppp);
            res = RG->GetRnd();
          }
          while (rep0 >= pos);
          rep0++;
        }

        for (UInt32 i = 0; i < len && pos < BufferSize; i++, pos++)
          Buffer[pos] = Buffer[pos - rep0];
      }
    }
  }
};


class CBenchmarkInStream:
  public ISequentialInStream,
  public CMyUnknownImp
{
  const Byte *Data;
  size_t Pos;
  size_t Size;
public:
  MY_UNKNOWN_IMP
  void Init(const Byte *data, size_t size)
  {
    Data = data;
    Size = size;
    Pos = 0;
  }
  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
};

STDMETHODIMP CBenchmarkInStream::Read(void *data, UInt32 size, UInt32 *processedSize)
{
  size_t remain = Size - Pos;
  UInt32 kMaxBlockSize = (1 << 20);
  if (size > kMaxBlockSize)
    size = kMaxBlockSize;
  if (size > remain)
    size = (UInt32)remain;
  for (UInt32 i = 0; i < size; i++)
    ((Byte *)data)[i] = Data[Pos + i];
  Pos += size;
  if(processedSize != NULL)
    *processedSize = size;
  return S_OK;
}
  
class CBenchmarkOutStream:
  public ISequentialOutStream,
  public CBenchBuffer,
  public CMyUnknownImp
{
  // bool _overflow;
public:
  UInt32 Pos;
  // CBenchmarkOutStream(): _overflow(false) {}
  void Init()
  {
    // _overflow = false;
    Pos = 0;
  }
  MY_UNKNOWN_IMP
  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
};

STDMETHODIMP CBenchmarkOutStream::Write(const void *data, UInt32 size, UInt32 *processedSize)
{
  size_t curSize = BufferSize - Pos;
  if (curSize > size)
    curSize = size;
  memcpy(Buffer + Pos, data, curSize);
  Pos += (UInt32)curSize;
  if(processedSize != NULL)
    *processedSize = (UInt32)curSize;
  if (curSize != size)
  {
    // _overflow = true;
    return E_FAIL;
  }
  return S_OK;
}
  
class CCrcOutStream:
  public ISequentialOutStream,
  public CMyUnknownImp
{
public:
  UInt32 Crc;
  MY_UNKNOWN_IMP
  void Init() { Crc = CRC_INIT_VAL; }
  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
};

STDMETHODIMP CCrcOutStream::Write(const void *data, UInt32 size, UInt32 *processedSize)
{
  Crc = CrcUpdate(Crc, data, size);
  if (processedSize != NULL)
    *processedSize = size;
  return S_OK;
}
  
static UInt64 GetTimeCount()
{
  #ifdef USE_POSIX_TIME
  #ifdef USE_POSIX_TIME2
  timeval v;
  if (gettimeofday(&v, 0) == 0)
    return (UInt64)(v.tv_sec) * 1000000 + v.tv_usec;
  return (UInt64)time(NULL) * 1000000;
  #else
  return time(NULL);
  #endif
  #else
  /*
  LARGE_INTEGER value;
  if (::QueryPerformanceCounter(&value))
    return value.QuadPart;
  */
  return GetTickCount();
  #endif
}

static UInt64 GetFreq()
{
  #ifdef USE_POSIX_TIME
  #ifdef USE_POSIX_TIME2
  return 1000000;
  #else
  return 1;
  #endif
  #else
  /*
  LARGE_INTEGER value;
  if (::QueryPerformanceFrequency(&value))
    return value.QuadPart;
  */
  return 1000;
  #endif
}

#ifndef USE_POSIX_TIME
static inline UInt64 GetTime64(const FILETIME &t) { return ((UInt64)t.dwHighDateTime << 32) | t.dwLowDateTime; }
#endif

static UInt64 GetUserTime()
{
  #ifdef USE_POSIX_TIME
  return clock();
  #else
  FILETIME creationTime, exitTime, kernelTime, userTime;
  if (
  #ifdef UNDER_CE
    ::GetThreadTimes(::GetCurrentThread()
  #else
    ::GetProcessTimes(::GetCurrentProcess()
  #endif
    , &creationTime, &exitTime, &kernelTime, &userTime) != 0)
    return GetTime64(userTime) + GetTime64(kernelTime);
  return (UInt64)GetTickCount() * 10000;
  #endif
}

static UInt64 GetUserFreq()
{
  #ifdef USE_POSIX_TIME
  return CLOCKS_PER_SEC;
  #else
  return 10000000;
  #endif
}

class CBenchProgressStatus
{
  #ifdef BENCH_MT
  NWindows::NSynchronization::CCriticalSection CS;
  #endif
public:
  HRESULT Res;
  bool EncodeMode;
  void SetResult(HRESULT res)
  {
    #ifdef BENCH_MT
    NWindows::NSynchronization::CCriticalSectionLock lock(CS);
    #endif
    Res = res;
  }
  HRESULT GetResult()
  {
    #ifdef BENCH_MT
    NWindows::NSynchronization::CCriticalSectionLock lock(CS);
    #endif
    return Res;
  }
};

class CBenchProgressInfo:
  public ICompressProgressInfo,
  public CMyUnknownImp
{
public:
  CBenchProgressStatus *Status;
  CBenchInfo BenchInfo;
  HRESULT Res;
  IBenchCallback *callback;
  CBenchProgressInfo(): callback(0) {}
  MY_UNKNOWN_IMP
  STDMETHOD(SetRatioInfo)(const UInt64 *inSize, const UInt64 *outSize);
};

static void SetStartTime(CBenchInfo &bi)
{
  bi.GlobalFreq = GetFreq();
  bi.UserFreq = GetUserFreq();
  bi.GlobalTime = ::GetTimeCount();
  bi.UserTime = ::GetUserTime();
}

static void SetFinishTime(const CBenchInfo &biStart, CBenchInfo &dest)
{
  dest.GlobalFreq = GetFreq();
  dest.UserFreq = GetUserFreq();
  dest.GlobalTime = ::GetTimeCount() - biStart.GlobalTime;
  dest.UserTime = ::GetUserTime() - biStart.UserTime;
}

STDMETHODIMP CBenchProgressInfo::SetRatioInfo(const UInt64 *inSize, const UInt64 *outSize)
{
  HRESULT res = Status->GetResult();
  if (res != S_OK)
    return res;
  if (!callback)
    return res;
  CBenchInfo info = BenchInfo;
  SetFinishTime(BenchInfo, info);
  if (Status->EncodeMode)
  {
    info.UnpackSize = *inSize;
    info.PackSize = *outSize;
    res = callback->SetEncodeResult(info, false);
  }
  else
  {
    info.PackSize = BenchInfo.PackSize + *inSize;
    info.UnpackSize = BenchInfo.UnpackSize + *outSize;
    res = callback->SetDecodeResult(info, false);
  }
  if (res != S_OK)
    Status->SetResult(res);
  return res;
}

static const int kSubBits = 8;

static UInt32 GetLogSize(UInt32 size)
{
  for (int i = kSubBits; i < 32; i++)
    for (UInt32 j = 0; j < (1 << kSubBits); j++)
      if (size <= (((UInt32)1) << i) + (j << (i - kSubBits)))
        return (i << kSubBits) + j;
  return (32 << kSubBits);
}

static void NormalizeVals(UInt64 &v1, UInt64 &v2)
{
  while (v1 > 1000000)
  {
    v1 >>= 1;
    v2 >>= 1;
  }
}

UInt64 GetUsage(const CBenchInfo &info)
{
  UInt64 userTime = info.UserTime;
  UInt64 userFreq = info.UserFreq;
  UInt64 globalTime = info.GlobalTime;
  UInt64 globalFreq = info.GlobalFreq;
  NormalizeVals(userTime, userFreq);
  NormalizeVals(globalFreq, globalTime);
  if (userFreq == 0)
    userFreq = 1;
  if (globalTime == 0)
    globalTime = 1;
  return userTime * globalFreq * 1000000 / userFreq / globalTime;
}

UInt64 GetRatingPerUsage(const CBenchInfo &info, UInt64 rating)
{
  UInt64 userTime = info.UserTime;
  UInt64 userFreq = info.UserFreq;
  UInt64 globalTime = info.GlobalTime;
  UInt64 globalFreq = info.GlobalFreq;
  NormalizeVals(userFreq, userTime);
  NormalizeVals(globalTime, globalFreq);
  if (globalFreq == 0)
    globalFreq = 1;
  if (userTime == 0)
    userTime = 1;
  return userFreq * globalTime / globalFreq *  rating / userTime;
}

static UInt64 MyMultDiv64(UInt64 value, UInt64 elapsedTime, UInt64 freq)
{
  UInt64 elTime = elapsedTime;
  NormalizeVals(freq, elTime);
  if (elTime == 0)
    elTime = 1;
  return value * freq / elTime;
}

UInt64 GetCompressRating(UInt32 dictionarySize, UInt64 elapsedTime, UInt64 freq, UInt64 size)
{
  UInt64 t = GetLogSize(dictionarySize) - (kBenchMinDicLogSize << kSubBits);
  UInt64 numCommandsForOne = 870 + ((t * t * 5) >> (2 * kSubBits));
  UInt64 numCommands = (UInt64)(size) * numCommandsForOne;
  return MyMultDiv64(numCommands, elapsedTime, freq);
}

UInt64 GetDecompressRating(UInt64 elapsedTime, UInt64 freq, UInt64 outSize, UInt64 inSize, UInt32 numIterations)
{
  UInt64 numCommands = (inSize * 200 + outSize * 4) * numIterations;
  return MyMultDiv64(numCommands, elapsedTime, freq);
}

#ifdef EXTERNAL_LZMA
typedef UInt32 (WINAPI * CreateObjectPointer)(const GUID *clsID,
    const GUID *interfaceID, void **outObject);
#endif

struct CEncoderInfo;

struct CEncoderInfo
{
  #ifdef BENCH_MT
  NWindows::CThread thread[2];
  #endif
  CMyComPtr<ICompressCoder> encoder;
  CBenchProgressInfo *progressInfoSpec[2];
  CMyComPtr<ICompressProgressInfo> progressInfo[2];
  UInt32 NumIterations;
  #ifdef USE_ALLOCA
  size_t AllocaSize;
  #endif

  struct CDecoderInfo
  {
    CEncoderInfo *Encoder;
    UInt32 DecoderIndex;
    #ifdef USE_ALLOCA
    size_t AllocaSize;
    #endif
    bool CallbackMode;
  };
  CDecoderInfo decodersInfo[2];

  CMyComPtr<ICompressCoder> decoders[2];
  HRESULT Results[2];
  CBenchmarkOutStream *outStreamSpec;
  CMyComPtr<ISequentialOutStream> outStream;
  IBenchCallback *callback;
  UInt32 crc;
  UInt32 kBufferSize;
  UInt32 compressedSize;
  CBenchRandomGenerator rg;
  CBenchmarkOutStream *propStreamSpec;
  CMyComPtr<ISequentialOutStream> propStream;
  HRESULT Init(UInt32 dictionarySize, UInt32 numThreads, CBaseRandomGenerator *rg);
  HRESULT Encode();
  HRESULT Decode(UInt32 decoderIndex);

  CEncoderInfo(): outStreamSpec(0), callback(0), propStreamSpec(0) {}

  #ifdef BENCH_MT
  static THREAD_FUNC_DECL EncodeThreadFunction(void *param)
  {
    CEncoderInfo *encoder = (CEncoderInfo *)param;
    #ifdef USE_ALLOCA
    alloca(encoder->AllocaSize);
    #endif
    HRESULT res = encoder->Encode();
    encoder->Results[0] = res;
    if (res != S_OK)
      encoder->progressInfoSpec[0]->Status->SetResult(res);

    return 0;
  }
  static THREAD_FUNC_DECL DecodeThreadFunction(void *param)
  {
    CDecoderInfo *decoder = (CDecoderInfo *)param;
    #ifdef USE_ALLOCA
    alloca(decoder->AllocaSize);
    #endif
    CEncoderInfo *encoder = decoder->Encoder;
    encoder->Results[decoder->DecoderIndex] = encoder->Decode(decoder->DecoderIndex);
    return 0;
  }

  HRESULT CreateEncoderThread()
  {
    return thread[0].Create(EncodeThreadFunction, this);
  }

  HRESULT CreateDecoderThread(int index, bool callbackMode
      #ifdef USE_ALLOCA
      , size_t allocaSize
      #endif
      )
  {
    CDecoderInfo &decoder = decodersInfo[index];
    decoder.DecoderIndex = index;
    decoder.Encoder = this;
    #ifdef USE_ALLOCA
    decoder.AllocaSize = allocaSize;
    #endif
    decoder.CallbackMode = callbackMode;
    return thread[index].Create(DecodeThreadFunction, &decoder);
  }
  #endif
};

HRESULT CEncoderInfo::Init(UInt32 dictionarySize, UInt32 numThreads, CBaseRandomGenerator *rgLoc)
{
  rg.Set(rgLoc);
  kBufferSize = dictionarySize + kAdditionalSize;
  UInt32 kCompressedBufferSize = (kBufferSize / 2) + kCompressedAdditionalSize;
  if (!rg.Alloc(kBufferSize))
    return E_OUTOFMEMORY;
  rg.Generate();
  crc = CrcCalc(rg.Buffer, rg.BufferSize);

  outStreamSpec = new CBenchmarkOutStream;
  if (!outStreamSpec->Alloc(kCompressedBufferSize))
    return E_OUTOFMEMORY;

  outStream = outStreamSpec;

  propStreamSpec = 0;
  if (!propStream)
  {
    propStreamSpec = new CBenchmarkOutStream;
    propStream = propStreamSpec;
  }
  if (!propStreamSpec->Alloc(kMaxLzmaPropSize))
    return E_OUTOFMEMORY;
  propStreamSpec->Init();
  
  PROPID propIDs[] =
  {
    NCoderPropID::kDictionarySize,
    NCoderPropID::kNumThreads
  };
  const int kNumProps = sizeof(propIDs) / sizeof(propIDs[0]);
  PROPVARIANT props[kNumProps];
  props[0].vt = VT_UI4;
  props[0].ulVal = dictionarySize;

  props[1].vt = VT_UI4;
  props[1].ulVal = numThreads;

  {
    CMyComPtr<ICompressSetCoderProperties> setCoderProperties;
    RINOK(encoder.QueryInterface(IID_ICompressSetCoderProperties, &setCoderProperties));
    if (!setCoderProperties)
      return E_FAIL;
    RINOK(setCoderProperties->SetCoderProperties(propIDs, props, kNumProps));

    CMyComPtr<ICompressWriteCoderProperties> writeCoderProperties;
    encoder.QueryInterface(IID_ICompressWriteCoderProperties, &writeCoderProperties);
    if (writeCoderProperties)
    {
      RINOK(writeCoderProperties->WriteCoderProperties(propStream));
    }
  }
  return S_OK;
}

HRESULT CEncoderInfo::Encode()
{
  CBenchmarkInStream *inStreamSpec = new CBenchmarkInStream;
  CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
  inStreamSpec->Init(rg.Buffer, rg.BufferSize);
  outStreamSpec->Init();

  RINOK(encoder->Code(inStream, outStream, 0, 0, progressInfo[0]));
  compressedSize = outStreamSpec->Pos;
  encoder.Release();
  return S_OK;
}

HRESULT CEncoderInfo::Decode(UInt32 decoderIndex)
{
  CBenchmarkInStream *inStreamSpec = new CBenchmarkInStream;
  CMyComPtr<ISequentialInStream> inStream = inStreamSpec;
  CMyComPtr<ICompressCoder> &decoder = decoders[decoderIndex];

  CMyComPtr<ICompressSetDecoderProperties2> compressSetDecoderProperties;
  decoder.QueryInterface(IID_ICompressSetDecoderProperties2, &compressSetDecoderProperties);
  if (!compressSetDecoderProperties)
    return E_FAIL;

  CCrcOutStream *crcOutStreamSpec = new CCrcOutStream;
  CMyComPtr<ISequentialOutStream> crcOutStream = crcOutStreamSpec;
    
  CBenchProgressInfo *pi = progressInfoSpec[decoderIndex];
  pi->BenchInfo.UnpackSize = 0;
  pi->BenchInfo.PackSize = 0;

  for (UInt32 j = 0; j < NumIterations; j++)
  {
    inStreamSpec->Init(outStreamSpec->Buffer, compressedSize);
    crcOutStreamSpec->Init();
    
    RINOK(compressSetDecoderProperties->SetDecoderProperties2(propStreamSpec->Buffer, propStreamSpec->Pos));
    UInt64 outSize = kBufferSize;
    RINOK(decoder->Code(inStream, crcOutStream, 0, &outSize, progressInfo[decoderIndex]));
    if (CRC_GET_DIGEST(crcOutStreamSpec->Crc) != crc)
      return S_FALSE;
    pi->BenchInfo.UnpackSize += kBufferSize;
    pi->BenchInfo.PackSize += compressedSize;
  }
  decoder.Release();
  return S_OK;
}

static const UInt32 kNumThreadsMax = (1 << 16);

struct CBenchEncoders
{
  CEncoderInfo *encoders;
  CBenchEncoders(UInt32 num): encoders(0) { encoders = new CEncoderInfo[num]; }
  ~CBenchEncoders() { delete []encoders; }
};

HRESULT LzmaBench(
  #ifdef EXTERNAL_LZMA
  CCodecs *codecs,
  #endif
  UInt32 numThreads, UInt32 dictionarySize, IBenchCallback *callback)
{
  UInt32 numEncoderThreads =
    #ifdef BENCH_MT
    (numThreads > 1 ? numThreads / 2 : 1);
    #else
    1;
    #endif
  UInt32 numSubDecoderThreads =
    #ifdef BENCH_MT
    (numThreads > 1 ? 2 : 1);
    #else
    1;
    #endif
  if (dictionarySize < (1 << kBenchMinDicLogSize) || numThreads < 1 || numEncoderThreads > kNumThreadsMax)
  {
    return E_INVALIDARG;
  }

  CBenchEncoders encodersSpec(numEncoderThreads);
  CEncoderInfo *encoders = encodersSpec.encoders;

  #ifdef EXTERNAL_LZMA
  UString name = L"LZMA";
  #endif

  UInt32 i;
  for (i = 0; i < numEncoderThreads; i++)
  {
    CEncoderInfo &encoder = encoders[i];
    encoder.callback = (i == 0) ? callback : 0;

    #ifdef EXTERNAL_LZMA
    RINOK(codecs->CreateCoder(name, true, encoder.encoder));
    #else
    encoder.encoder = new NCompress::NLzma::CEncoder;
    #endif
    for (UInt32 j = 0; j < numSubDecoderThreads; j++)
    {
      #ifdef EXTERNAL_LZMA
      RINOK(codecs->CreateCoder(name, false, encoder.decoders[j]));
      #else
      encoder.decoders[j] = new NCompress::NLzma::CDecoder;
      #endif
    }
  }

  CBaseRandomGenerator rg;
  rg.Init();
  for (i = 0; i < numEncoderThreads; i++)
  {
    RINOK(encoders[i].Init(dictionarySize, numThreads, &rg));
  }

  CBenchProgressStatus status;
  status.Res = S_OK;
  status.EncodeMode = true;

  for (i = 0; i < numEncoderThreads; i++)
  {
    CEncoderInfo &encoder = encoders[i];
    for (int j = 0; j < 2; j++)
    {
      encoder.progressInfo[j] = encoder.progressInfoSpec[j] = new CBenchProgressInfo;
      encoder.progressInfoSpec[j]->Status = &status;
    }
    if (i == 0)
    {
      encoder.progressInfoSpec[0]->callback = callback;
      encoder.progressInfoSpec[0]->BenchInfo.NumIterations = numEncoderThreads;
      SetStartTime(encoder.progressInfoSpec[0]->BenchInfo);
    }

    #ifdef BENCH_MT
    if (numEncoderThreads > 1)
    {
      #ifdef USE_ALLOCA
      encoder.AllocaSize = (i * 16 * 21) & 0x7FF;
      #endif
      RINOK(encoder.CreateEncoderThread())
    }
    else
    #endif
    {
      RINOK(encoder.Encode());
    }
  }
  #ifdef BENCH_MT
  if (numEncoderThreads > 1)
    for (i = 0; i < numEncoderThreads; i++)
      encoders[i].thread[0].Wait();
  #endif

  RINOK(status.Res);

  CBenchInfo info;

  SetFinishTime(encoders[0].progressInfoSpec[0]->BenchInfo, info);
  info.UnpackSize = 0;
  info.PackSize = 0;
  info.NumIterations = 1; // progressInfoSpec->NumIterations;
  for (i = 0; i < numEncoderThreads; i++)
  {
    CEncoderInfo &encoder = encoders[i];
    info.UnpackSize += encoder.kBufferSize;
    info.PackSize += encoder.compressedSize;
  }
  RINOK(callback->SetEncodeResult(info, true));


  status.Res = S_OK;
  status.EncodeMode = false;

  UInt32 numDecoderThreads = numEncoderThreads * numSubDecoderThreads;
  for (i = 0; i < numEncoderThreads; i++)
  {
    CEncoderInfo &encoder = encoders[i];
    encoder.NumIterations = 2 + kUncompressMinBlockSize / encoder.kBufferSize;

    if (i == 0)
    {
      encoder.progressInfoSpec[0]->callback = callback;
      encoder.progressInfoSpec[0]->BenchInfo.NumIterations = numDecoderThreads;
      SetStartTime(encoder.progressInfoSpec[0]->BenchInfo);
    }

    #ifdef BENCH_MT
    if (numDecoderThreads > 1)
    {
      for (UInt32 j = 0; j < numSubDecoderThreads; j++)
      {
        HRESULT res = encoder.CreateDecoderThread(j, (i == 0 && j == 0)
            #ifdef USE_ALLOCA
            , ((i * numSubDecoderThreads + j) * 16 * 21) & 0x7FF
            #endif
            );
        RINOK(res);
      }
    }
    else
    #endif
    {
      RINOK(encoder.Decode(0));
    }
  }
  #ifdef BENCH_MT
  HRESULT res = S_OK;
  if (numDecoderThreads > 1)
    for (i = 0; i < numEncoderThreads; i++)
      for (UInt32 j = 0; j < numSubDecoderThreads; j++)
      {
        CEncoderInfo &encoder = encoders[i];
        encoder.thread[j].Wait();
        if (encoder.Results[j] != S_OK)
          res = encoder.Results[j];
      }
  RINOK(res);
  #endif
  RINOK(status.Res);
  SetFinishTime(encoders[0].progressInfoSpec[0]->BenchInfo, info);
  #ifdef BENCH_MT
  #ifdef UNDER_CE
  if (numDecoderThreads > 1)
    for (i = 0; i < numEncoderThreads; i++)
      for (UInt32 j = 0; j < numSubDecoderThreads; j++)
      {
        FILETIME creationTime, exitTime, kernelTime, userTime;
        if (::GetThreadTimes(encoders[i].thread[j], &creationTime, &exitTime, &kernelTime, &userTime) != 0)
          info.UserTime += GetTime64(userTime) + GetTime64(kernelTime);
      }
  #endif
  #endif
  info.UnpackSize = 0;
  info.PackSize = 0;
  info.NumIterations = numSubDecoderThreads * encoders[0].NumIterations;
  for (i = 0; i < numEncoderThreads; i++)
  {
    CEncoderInfo &encoder = encoders[i];
    info.UnpackSize += encoder.kBufferSize;
    info.PackSize += encoder.compressedSize;
  }
  RINOK(callback->SetDecodeResult(info, false));
  RINOK(callback->SetDecodeResult(info, true));
  return S_OK;
}


inline UInt64 GetLZMAUsage(bool multiThread, UInt32 dictionary)
{
  UInt32 hs = dictionary - 1;
  hs |= (hs >> 1);
  hs |= (hs >> 2);
  hs |= (hs >> 4);
  hs |= (hs >> 8);
  hs >>= 1;
  hs |= 0xFFFF;
  if (hs > (1 << 24))
    hs >>= 1;
  hs++;
  return ((hs + (1 << 16)) + (UInt64)dictionary * 2) * 4 + (UInt64)dictionary * 3 / 2 +
      (1 << 20) + (multiThread ? (6 << 20) : 0);
}

UInt64 GetBenchMemoryUsage(UInt32 numThreads, UInt32 dictionary)
{
  const UInt32 kBufferSize = dictionary;
  const UInt32 kCompressedBufferSize = (kBufferSize / 2);
  UInt32 numSubThreads = (numThreads > 1) ? 2 : 1;
  UInt32 numBigThreads = numThreads / numSubThreads;
  return (kBufferSize + kCompressedBufferSize +
    GetLZMAUsage((numThreads > 1), dictionary) + (2 << 20)) * numBigThreads;
}

static bool CrcBig(const void *data, UInt32 size, UInt32 numCycles, UInt32 crcBase)
{
  for (UInt32 i = 0; i < numCycles; i++)
    if (CrcCalc(data, size) != crcBase)
      return false;
  return true;
}

#ifdef BENCH_MT
struct CCrcInfo
{
  NWindows::CThread Thread;
  const Byte *Data;
  UInt32 Size;
  UInt32 NumCycles;
  UInt32 Crc;
  bool Res;
  void Wait()
  {
    Thread.Wait();
    Thread.Close();
  }
};

static THREAD_FUNC_DECL CrcThreadFunction(void *param)
{
  CCrcInfo *p = (CCrcInfo *)param;
  p->Res = CrcBig(p->Data, p->Size, p->NumCycles, p->Crc);
  return 0;
}

struct CCrcThreads
{
  UInt32 NumThreads;
  CCrcInfo *Items;
  CCrcThreads(): Items(0), NumThreads(0) {}
  void WaitAll()
  {
    for (UInt32 i = 0; i < NumThreads; i++)
      Items[i].Wait();
    NumThreads = 0;
  }
  ~CCrcThreads()
  {
    WaitAll();
    delete []Items;
  }
};
#endif

static UInt32 CrcCalc1(const Byte *buf, UInt32 size)
{
  UInt32 crc = CRC_INIT_VAL;;
  for (UInt32 i = 0; i < size; i++)
    crc = CRC_UPDATE_BYTE(crc, buf[i]);
  return CRC_GET_DIGEST(crc);
}

static void RandGen(Byte *buf, UInt32 size, CBaseRandomGenerator &RG)
{
  for (UInt32 i = 0; i < size; i++)
    buf[i] = (Byte)RG.GetRnd();
}

static UInt32 RandGenCrc(Byte *buf, UInt32 size, CBaseRandomGenerator &RG)
{
  RandGen(buf, size, RG);
  return CrcCalc1(buf, size);
}

bool CrcInternalTest()
{
  CBenchBuffer buffer;
  const UInt32 kBufferSize0 = (1 << 8);
  const UInt32 kBufferSize1 = (1 << 10);
  const UInt32 kCheckSize = (1 << 5);
  if (!buffer.Alloc(kBufferSize0 + kBufferSize1))
    return false;
  Byte *buf = buffer.Buffer;
  UInt32 i;
  for (i = 0; i < kBufferSize0; i++)
    buf[i] = (Byte)i;
  UInt32 crc1 = CrcCalc1(buf, kBufferSize0);
  if (crc1 != 0x29058C73)
    return false;
  CBaseRandomGenerator RG;
  RandGen(buf + kBufferSize0, kBufferSize1, RG);
  for (i = 0; i < kBufferSize0 + kBufferSize1 - kCheckSize; i++)
    for (UInt32 j = 0; j < kCheckSize; j++)
      if (CrcCalc1(buf + i, j) != CrcCalc(buf + i, j))
        return false;
  return true;
}

HRESULT CrcBench(UInt32 numThreads, UInt32 bufferSize, UInt64 &speed)
{
  if (numThreads == 0)
    numThreads = 1;

  CBenchBuffer buffer;
  size_t totalSize = (size_t)bufferSize * numThreads;
  if (totalSize / numThreads != bufferSize)
    return E_OUTOFMEMORY;
  if (!buffer.Alloc(totalSize))
    return E_OUTOFMEMORY;

  Byte *buf = buffer.Buffer;
  CBaseRandomGenerator RG;
  UInt32 numCycles = (kCrcBlockSize) / ((bufferSize >> 2) + 1) + 1;

  UInt64 timeVal;
  #ifdef BENCH_MT
  CCrcThreads threads;
  if (numThreads > 1)
  {
    threads.Items = new CCrcInfo[numThreads];
    UInt32 i;
    for (i = 0; i < numThreads; i++)
    {
      CCrcInfo &info = threads.Items[i];
      Byte *data = buf + (size_t)bufferSize * i;
      info.Data = data;
      info.NumCycles = numCycles;
      info.Size = bufferSize;
      info.Crc = RandGenCrc(data, bufferSize, RG);
    }
    timeVal = GetTimeCount();
    for (i = 0; i < numThreads; i++)
    {
      CCrcInfo &info = threads.Items[i];
      RINOK(info.Thread.Create(CrcThreadFunction, &info));
      threads.NumThreads++;
    }
    threads.WaitAll();
    for (i = 0; i < numThreads; i++)
      if (!threads.Items[i].Res)
        return S_FALSE;
  }
  else
  #endif
  {
    UInt32 crc = RandGenCrc(buf, bufferSize, RG);
    timeVal = GetTimeCount();
    if (!CrcBig(buf, bufferSize, numCycles, crc))
      return S_FALSE;
  }
  timeVal = GetTimeCount() - timeVal;
  if (timeVal == 0)
    timeVal = 1;

  UInt64 size = (UInt64)numCycles * totalSize;
  speed = MyMultDiv64(size, timeVal, GetFreq());
  return S_OK;
}

