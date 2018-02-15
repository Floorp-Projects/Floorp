// Lzma2Decoder.cpp

#include "StdAfx.h"

#include "../../../C/Alloc.h"

#include "../Common/StreamUtils.h"

#include "Lzma2Decoder.h"

static HRESULT SResToHRESULT(SRes res)
{
  switch (res)
  {
    case SZ_OK: return S_OK;
    case SZ_ERROR_MEM: return E_OUTOFMEMORY;
    case SZ_ERROR_PARAM: return E_INVALIDARG;
    case SZ_ERROR_UNSUPPORTED: return E_NOTIMPL;
    case SZ_ERROR_DATA: return S_FALSE;
  }
  return E_FAIL;
}

namespace NCompress {
namespace NLzma2 {

CDecoder::CDecoder():
    _inBuf(NULL),
    _finishMode(false),
    _outSizeDefined(false),
    _outStep(1 << 22),
    _inBufSize(0),
    _inBufSizeNew(1 << 20)
{
  Lzma2Dec_Construct(&_state);
}

CDecoder::~CDecoder()
{
  Lzma2Dec_Free(&_state, &g_Alloc);
  MidFree(_inBuf);
}

STDMETHODIMP CDecoder::SetInBufSize(UInt32 , UInt32 size) { _inBufSizeNew = size; return S_OK; }
STDMETHODIMP CDecoder::SetOutBufSize(UInt32 , UInt32 size) { _outStep = size; return S_OK; }

STDMETHODIMP CDecoder::SetDecoderProperties2(const Byte *prop, UInt32 size)
{
  if (size != 1)
    return E_NOTIMPL;
  
  RINOK(SResToHRESULT(Lzma2Dec_Allocate(&_state, prop[0], &g_Alloc)));
  
  if (!_inBuf || _inBufSize != _inBufSizeNew)
  {
    MidFree(_inBuf);
    _inBufSize = 0;
    _inBuf = (Byte *)MidAlloc(_inBufSizeNew);
    if (!_inBuf)
      return E_OUTOFMEMORY;
    _inBufSize = _inBufSizeNew;
  }

  return S_OK;
}


STDMETHODIMP CDecoder::SetOutStreamSize(const UInt64 *outSize)
{
  _outSizeDefined = (outSize != NULL);
  _outSize = 0;
  if (_outSizeDefined)
    _outSize = *outSize;
  _inPos = _inLim = 0;
  _inProcessed = 0;
  _outProcessed = 0;

  Lzma2Dec_Init(&_state);

  return S_OK;
}


STDMETHODIMP CDecoder::SetFinishMode(UInt32 finishMode)
{
  _finishMode = (finishMode != 0);
  return S_OK;
}


STDMETHODIMP CDecoder::GetInStreamProcessedSize(UInt64 *value)
{
  *value = _inProcessed;
  return S_OK;
}


STDMETHODIMP CDecoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress)
{
  if (!_inBuf)
    return S_FALSE;

  SetOutStreamSize(outSize);

  SizeT wrPos = _state.decoder.dicPos;
  HRESULT readRes = S_OK;

  for (;;)
  {
    if (_inPos == _inLim && readRes == S_OK)
    {
      _inPos = _inLim = 0;
      readRes = inStream->Read(_inBuf, _inBufSize, &_inLim);
    }

    const SizeT dicPos = _state.decoder.dicPos;
    SizeT size;
    {
      SizeT next = _state.decoder.dicBufSize;
      if (next - wrPos > _outStep)
        next = wrPos + _outStep;
      size = next - dicPos;
    }

    ELzmaFinishMode finishMode = LZMA_FINISH_ANY;
    if (_outSizeDefined)
    {
      const UInt64 rem = _outSize - _outProcessed;
      if (size >= rem)
      {
        size = (SizeT)rem;
        if (_finishMode)
          finishMode = LZMA_FINISH_END;
      }
    }

    SizeT inProcessed = _inLim - _inPos;
    ELzmaStatus status;
    
    SRes res = Lzma2Dec_DecodeToDic(&_state, dicPos + size, _inBuf + _inPos, &inProcessed, finishMode, &status);

    
    _inPos += (UInt32)inProcessed;
    _inProcessed += inProcessed;
    const SizeT outProcessed = _state.decoder.dicPos - dicPos;
    _outProcessed += outProcessed;

    
    bool outFinished = (_outSizeDefined && _outProcessed >= _outSize);

    bool needStop = (res != 0
        || (inProcessed == 0 && outProcessed == 0)
        || status == LZMA_STATUS_FINISHED_WITH_MARK
        || (!_finishMode && outFinished));

    if (needStop || outProcessed >= size)
    {
      HRESULT res2 = WriteStream(outStream, _state.decoder.dic + wrPos, _state.decoder.dicPos - wrPos);

      if (_state.decoder.dicPos == _state.decoder.dicBufSize)
        _state.decoder.dicPos = 0;
      wrPos = _state.decoder.dicPos;

      RINOK(res2);

      if (needStop)
      {
        if (res != 0)
          return S_FALSE;

        if (status == LZMA_STATUS_FINISHED_WITH_MARK)
        {
          if (_finishMode)
          {
            if (inSize && *inSize != _inProcessed)
              return S_FALSE;
            if (_outSizeDefined && _outSize != _outProcessed)
              return S_FALSE;
          }
          return readRes;
        }

        if (!_finishMode && outFinished)
          return readRes;
   
        return S_FALSE;
      }
    }
    
    if (progress)
    {
      RINOK(progress->SetRatioInfo(&_inProcessed, &_outProcessed));
    }
  }
}


#ifndef NO_READ_FROM_CODER

STDMETHODIMP CDecoder::SetInStream(ISequentialInStream *inStream) { _inStream = inStream; return S_OK; }
STDMETHODIMP CDecoder::ReleaseInStream() { _inStream.Release(); return S_OK; }
  

STDMETHODIMP CDecoder::Read(void *data, UInt32 size, UInt32 *processedSize)
{
  if (processedSize)
    *processedSize = 0;

  ELzmaFinishMode finishMode = LZMA_FINISH_ANY;
  if (_outSizeDefined)
  {
    const UInt64 rem = _outSize - _outProcessed;
    if (size >= rem)
    {
      size = (UInt32)rem;
      if (_finishMode)
        finishMode = LZMA_FINISH_END;
    }
  }

  HRESULT readRes = S_OK;

  for (;;)
  {
    if (_inPos == _inLim && readRes == S_OK)
    {
      _inPos = _inLim = 0;
      readRes = _inStream->Read(_inBuf, _inBufSize, &_inLim);
    }
    
    SizeT inProcessed = _inLim - _inPos;
    SizeT outProcessed = size;
    ELzmaStatus status;

    SRes res = Lzma2Dec_DecodeToBuf(&_state, (Byte *)data, &outProcessed,
        _inBuf + _inPos, &inProcessed, finishMode, &status);
    
    
    _inPos += (UInt32)inProcessed;
    _inProcessed += inProcessed;
    _outProcessed += outProcessed;
    size -= (UInt32)outProcessed;
    data = (Byte *)data + outProcessed;
    if (processedSize)
      *processedSize += (UInt32)outProcessed;
    
    if (res != 0)
      return S_FALSE;
    
    /*
    if (status == LZMA_STATUS_FINISHED_WITH_MARK)
      return readRes;

    if (size == 0 && status != LZMA_STATUS_NEEDS_MORE_INPUT)
    {
      if (_finishMode && _outSizeDefined && _outProcessed >= _outSize)
        return S_FALSE;
      return readRes;
    }
    */

    if (inProcessed == 0 && outProcessed == 0)
      return readRes;
  }
}

#endif

}}
