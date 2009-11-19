// CWrappers.h

#include "StdAfx.h"

#include "CWrappers.h"

#include "StreamUtils.h"

#define PROGRESS_UNKNOWN_VALUE ((UInt64)(Int64)-1)

#define CONVERT_PR_VAL(x) (x == PROGRESS_UNKNOWN_VALUE ? NULL : &x)

static SRes CompressProgress(void *pp, UInt64 inSize, UInt64 outSize)
{
  CCompressProgressWrap *p = (CCompressProgressWrap *)pp;
  p->Res = p->Progress->SetRatioInfo(CONVERT_PR_VAL(inSize), CONVERT_PR_VAL(outSize));
  return (SRes)p->Res;
}

CCompressProgressWrap::CCompressProgressWrap(ICompressProgressInfo *progress)
{
  p.Progress = CompressProgress;
  Progress = progress;
  Res = SZ_OK;
}

static const UInt32 kStreamStepSize = (UInt32)1 << 31;

SRes HRESULT_To_SRes(HRESULT res, SRes defaultRes)
{
  switch(res)
  {
    case S_OK: return SZ_OK;
    case E_OUTOFMEMORY: return SZ_ERROR_MEM;
    case E_INVALIDARG: return SZ_ERROR_PARAM;
    case E_ABORT: return SZ_ERROR_PROGRESS;
    case S_FALSE: return SZ_ERROR_DATA;
  }
  return defaultRes;
}

static SRes MyRead(void *object, void *data, size_t *size)
{
  CSeqInStreamWrap *p = (CSeqInStreamWrap *)object;
  UInt32 curSize = ((*size < kStreamStepSize) ? (UInt32)*size : kStreamStepSize);
  p->Res = (p->Stream->Read(data, curSize, &curSize));
  *size = curSize;
  if (p->Res == S_OK)
    return SZ_OK;
  return HRESULT_To_SRes(p->Res, SZ_ERROR_READ);
}

static size_t MyWrite(void *object, const void *data, size_t size)
{
  CSeqOutStreamWrap *p = (CSeqOutStreamWrap *)object;
  if (p->Stream)
  {
    p->Res = WriteStream(p->Stream, data, size);
    if (p->Res != 0)
      return 0;
  }
  else
    p->Res = S_OK;
  p->Processed += size;
  return size;
}

CSeqInStreamWrap::CSeqInStreamWrap(ISequentialInStream *stream)
{
  p.Read = MyRead;
  Stream = stream;
}

CSeqOutStreamWrap::CSeqOutStreamWrap(ISequentialOutStream *stream)
{
  p.Write = MyWrite;
  Stream = stream;
  Res = SZ_OK;
  Processed = 0;
}

HRESULT SResToHRESULT(SRes res)
{
  switch(res)
  {
    case SZ_OK: return S_OK;
    case SZ_ERROR_MEM: return E_OUTOFMEMORY;
    case SZ_ERROR_PARAM: return E_INVALIDARG;
    case SZ_ERROR_PROGRESS: return E_ABORT;
    case SZ_ERROR_DATA: return S_FALSE;
  }
  return E_FAIL;
}

static SRes InStreamWrap_Read(void *pp, void *data, size_t *size)
{
  CSeekInStreamWrap *p = (CSeekInStreamWrap *)pp;
  UInt32 curSize = ((*size < kStreamStepSize) ? (UInt32)*size : kStreamStepSize);
  p->Res = p->Stream->Read(data, curSize, &curSize);
  *size = curSize;
  return (p->Res == S_OK) ? SZ_OK : SZ_ERROR_READ;
}

static SRes InStreamWrap_Seek(void *pp, Int64 *offset, ESzSeek origin)
{
  CSeekInStreamWrap *p = (CSeekInStreamWrap *)pp;
  UInt32 moveMethod;
  switch(origin)
  {
    case SZ_SEEK_SET: moveMethod = STREAM_SEEK_SET; break;
    case SZ_SEEK_CUR: moveMethod = STREAM_SEEK_CUR; break;
    case SZ_SEEK_END: moveMethod = STREAM_SEEK_END; break;
    default: return SZ_ERROR_PARAM;
  }
  UInt64 newPosition;
  p->Res = p->Stream->Seek(*offset, moveMethod, &newPosition);
  *offset = (Int64)newPosition;
  return (p->Res == S_OK) ? SZ_OK : SZ_ERROR_READ;
}

CSeekInStreamWrap::CSeekInStreamWrap(IInStream *stream)
{
  Stream = stream;
  p.Read = InStreamWrap_Read;
  p.Seek = InStreamWrap_Seek;
  Res = S_OK;
}
