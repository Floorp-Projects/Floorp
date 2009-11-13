// LzmaBenchCon.cpp

#include "StdAfx.h"

#include <stdio.h>

#include "LzmaBench.h"
#include "LzmaBenchCon.h"
#include "../../../Common/IntToString.h"

#if defined(BENCH_MT) || defined(_WIN32)
#include "../../../Windows/System.h"
#endif

#ifdef BREAK_HANDLER
#include "../../UI/Console/ConsoleClose.h"
#endif
#include "../../../Common/MyCom.h"

struct CTotalBenchRes
{
  UInt64 NumIterations;
  UInt64 Rating;
  UInt64 Usage;
  UInt64 RPU;
  void Init() { NumIterations = 0; Rating = 0; Usage = 0; RPU = 0; }
  void Normalize()
  {
    if (NumIterations == 0)
      return;
    Rating /= NumIterations;
    Usage /= NumIterations;
    RPU /= NumIterations;
    NumIterations = 1;
  }
  void SetMid(const CTotalBenchRes &r1, const CTotalBenchRes &r2)
  {
    Rating = (r1.Rating + r2.Rating) / 2;
    Usage = (r1.Usage + r2.Usage) / 2;
    RPU = (r1.RPU + r2.RPU) / 2;
    NumIterations = (r1.NumIterations + r2.NumIterations) / 2;
  }
};

struct CBenchCallback: public IBenchCallback
{
  CTotalBenchRes EncodeRes;
  CTotalBenchRes DecodeRes;
  FILE *f;
  void Init() { EncodeRes.Init(); DecodeRes.Init(); }
  void Normalize() { EncodeRes.Normalize(); DecodeRes.Normalize(); }
  UInt32 dictionarySize;
  HRESULT SetEncodeResult(const CBenchInfo &info, bool final);
  HRESULT SetDecodeResult(const CBenchInfo &info, bool final);
};

static void NormalizeVals(UInt64 &v1, UInt64 &v2)
{
  while (v1 > 1000000)
  {
    v1 >>= 1;
    v2 >>= 1;
  }
}

static UInt64 MyMultDiv64(UInt64 value, UInt64 elapsedTime, UInt64 freq)
{
  UInt64 elTime = elapsedTime;
  NormalizeVals(freq, elTime);
  if (elTime == 0)
    elTime = 1;
  return value * freq / elTime;
}

static void PrintNumber(FILE *f, UInt64 value, int size)
{
  char s[32];
  ConvertUInt64ToString(value, s);
  fprintf(f, " ");
  for (int len = (int)strlen(s); len < size; len++)
    fprintf(f, " ");
  fprintf(f, "%s", s);
}

static void PrintRating(FILE *f, UInt64 rating)
{
  PrintNumber(f, rating / 1000000, 6);
}

static void PrintResults(FILE *f, UInt64 usage, UInt64 rpu, UInt64 rating)
{
  PrintNumber(f, (usage + 5000) / 10000, 5);
  PrintRating(f, rpu);
  PrintRating(f, rating);
}


static void PrintResults(FILE *f, const CBenchInfo &info, UInt64 rating, CTotalBenchRes &res)
{
  UInt64 speed = MyMultDiv64(info.UnpackSize, info.GlobalTime, info.GlobalFreq);
  PrintNumber(f, speed / 1024, 7);
  UInt64 usage = GetUsage(info);
  UInt64 rpu = GetRatingPerUsage(info, rating);
  PrintResults(f, usage, rpu, rating);
  res.NumIterations++;
  res.RPU += rpu;
  res.Rating += rating;
  res.Usage += usage;
}

static void PrintTotals(FILE *f, const CTotalBenchRes &res)
{
  fprintf(f, "       ");
  PrintResults(f, res.Usage, res.RPU, res.Rating);
}


HRESULT CBenchCallback::SetEncodeResult(const CBenchInfo &info, bool final)
{
  #ifdef BREAK_HANDLER
  if (NConsoleClose::TestBreakSignal())
    return E_ABORT;
  #endif

  if (final)
  {
    UInt64 rating = GetCompressRating(dictionarySize, info.GlobalTime, info.GlobalFreq, info.UnpackSize);
    PrintResults(f, info, rating, EncodeRes);
  }
  return S_OK;
}

static const char *kSep = "  | ";


HRESULT CBenchCallback::SetDecodeResult(const CBenchInfo &info, bool final)
{
  #ifdef BREAK_HANDLER
  if (NConsoleClose::TestBreakSignal())
    return E_ABORT;
  #endif
  if (final)
  {
    UInt64 rating = GetDecompressRating(info.GlobalTime, info.GlobalFreq, info.UnpackSize, info.PackSize, info.NumIterations);
    fprintf(f, kSep);
    CBenchInfo info2 = info;
    info2.UnpackSize *= info2.NumIterations;
    info2.PackSize *= info2.NumIterations;
    info2.NumIterations = 1;
    PrintResults(f, info2, rating, DecodeRes);
  }
  return S_OK;
}

static void PrintRequirements(FILE *f, const char *sizeString, UInt64 size, const char *threadsString, UInt32 numThreads)
{
  fprintf(f, "\nRAM %s ", sizeString);
  PrintNumber(f, (size >> 20), 5);
  fprintf(f, " MB,  # %s %3d", threadsString, (unsigned int)numThreads);
}

HRESULT LzmaBenchCon(
  #ifdef EXTERNAL_LZMA
  CCodecs *codecs,
  #endif
  FILE *f, UInt32 numIterations, UInt32 numThreads, UInt32 dictionary)
{
  if (!CrcInternalTest())
    return S_FALSE;
  #ifdef BENCH_MT
  UInt64 ramSize = NWindows::NSystem::GetRamSize();  //
  UInt32 numCPUs = NWindows::NSystem::GetNumberOfProcessors();
  PrintRequirements(f, "size: ", ramSize, "CPU hardware threads:", numCPUs);
  if (numThreads == (UInt32)-1)
    numThreads = numCPUs;
  if (numThreads > 1)
    numThreads &= ~1;
  if (dictionary == (UInt32)-1)
  {
    int dicSizeLog;
    for (dicSizeLog = 25; dicSizeLog > kBenchMinDicLogSize; dicSizeLog--)
      if (GetBenchMemoryUsage(numThreads, ((UInt32)1 << dicSizeLog)) + (8 << 20) <= ramSize)
        break;
    dictionary = (1 << dicSizeLog);
  }
  #else
  if (dictionary == (UInt32)-1)
    dictionary = (1 << 22);
  numThreads = 1;
  #endif

  PrintRequirements(f, "usage:", GetBenchMemoryUsage(numThreads, dictionary), "Benchmark threads:   ", numThreads);

  CBenchCallback callback;
  callback.Init();
  callback.f = f;
  
  fprintf(f, "\n\nDict        Compressing          |        Decompressing\n   ");
  int j;
  for (j = 0; j < 2; j++)
  {
    fprintf(f, "   Speed Usage    R/U Rating");
    if (j == 0)
      fprintf(f, kSep);
  }
  fprintf(f, "\n   ");
  for (j = 0; j < 2; j++)
  {
    fprintf(f, "    KB/s     %%   MIPS   MIPS");
    if (j == 0)
      fprintf(f, kSep);
  }
  fprintf(f, "\n\n");
  for (UInt32 i = 0; i < numIterations; i++)
  {
    const int kStartDicLog = 22;
    int pow = (dictionary < ((UInt32)1 << kStartDicLog)) ? kBenchMinDicLogSize : kStartDicLog;
    while (((UInt32)1 << pow) > dictionary)
      pow--;
    for (; ((UInt32)1 << pow) <= dictionary; pow++)
    {
      fprintf(f, "%2d:", pow);
      callback.dictionarySize = (UInt32)1 << pow;
      HRESULT res = LzmaBench(
        #ifdef EXTERNAL_LZMA
        codecs,
        #endif
        numThreads, callback.dictionarySize, &callback);
      fprintf(f, "\n");
      RINOK(res);
    }
  }
  callback.Normalize();
  fprintf(f, "----------------------------------------------------------------\nAvr:");
  PrintTotals(f, callback.EncodeRes);
  fprintf(f, "     ");
  PrintTotals(f, callback.DecodeRes);
  fprintf(f, "\nTot:");
  CTotalBenchRes midRes;
  midRes.SetMid(callback.EncodeRes, callback.DecodeRes);
  PrintTotals(f, midRes);
  fprintf(f, "\n");
  return S_OK;
}

struct CTempValues
{
  UInt64 *Values;
  CTempValues(UInt32 num) { Values = new UInt64[num]; }
  ~CTempValues() { delete []Values; }
};

HRESULT CrcBenchCon(FILE *f, UInt32 numIterations, UInt32 numThreads, UInt32 dictionary)
{
  if (!CrcInternalTest())
    return S_FALSE;

  #ifdef BENCH_MT
  UInt64 ramSize = NWindows::NSystem::GetRamSize();
  UInt32 numCPUs = NWindows::NSystem::GetNumberOfProcessors();
  PrintRequirements(f, "size: ", ramSize, "CPU hardware threads:", numCPUs);
  if (numThreads == (UInt32)-1)
    numThreads = numCPUs;
  #else
  numThreads = 1;
  #endif
  if (dictionary == (UInt32)-1)
    dictionary = (1 << 24);

  CTempValues speedTotals(numThreads);
  fprintf(f, "\n\nSize");
  for (UInt32 ti = 0; ti < numThreads; ti++)
  {
    fprintf(f, " %5d", ti + 1);
    speedTotals.Values[ti] = 0;
  }
  fprintf(f, "\n\n");

  UInt64 numSteps = 0;
  for (UInt32 i = 0; i < numIterations; i++)
  {
    for (int pow = 10; pow < 32; pow++)
    {
      UInt32 bufSize = (UInt32)1 << pow;
      if (bufSize > dictionary)
        break;
      fprintf(f, "%2d: ", pow);
      UInt64 speed;
      for (UInt32 ti = 0; ti < numThreads; ti++)
      {
        #ifdef BREAK_HANDLER
        if (NConsoleClose::TestBreakSignal())
          return E_ABORT;
        #endif
        RINOK(CrcBench(ti + 1, bufSize, speed));
        PrintNumber(f, (speed >> 20), 5);
        speedTotals.Values[ti] += speed;
      }
      fprintf(f, "\n");
      numSteps++;
    }
  }
  if (numSteps != 0)
  {
    fprintf(f, "\nAvg:");
    for (UInt32 ti = 0; ti < numThreads; ti++)
      PrintNumber(f, ((speedTotals.Values[ti] / numSteps) >> 20), 5);
    fprintf(f, "\n");
  }
  return S_OK;
}
