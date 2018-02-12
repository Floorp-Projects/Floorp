// XzDecoder.cpp

#include "StdAfx.h"

#include "../../../C/Alloc.h"

#include "../Common/StreamUtils.h"

#include "../Archive/IArchive.h"

#include "XzDecoder.h"

using namespace NArchive;

namespace NCompress {
namespace NXz {


CXzUnpackerCPP::CXzUnpackerCPP(): InBuf(NULL), OutBuf(NULL)
{
  XzUnpacker_Construct(&p, &g_Alloc);
}

CXzUnpackerCPP::~CXzUnpackerCPP()
{
  XzUnpacker_Free(&p);
  MidFree(InBuf);
  MidFree(OutBuf);
}


void CStatInfo::Clear()
{
  InSize = 0;
  OutSize = 0;
  PhySize = 0;
  
  NumStreams = 0;
  NumBlocks = 0;
  
  UnpackSize_Defined = false;
  
  NumStreams_Defined = false;
  NumBlocks_Defined = false;
  
  IsArc = false;
  UnexpectedEnd = false;
  DataAfterEnd = false;
  Unsupported = false;
  HeadersError = false;
  DataError = false;
  CrcError = false;
}


HRESULT CDecoder::Decode(ISequentialInStream *seqInStream, ISequentialOutStream *outStream,
    const UInt64 *outSizeLimit, bool finishStream, ICompressProgressInfo *progress)
{
  const size_t kInBufSize = (size_t)1 << 20;
  const size_t kOutBufSize = (size_t)1 << 21;

  Clear();
  DecodeRes = SZ_OK;

  XzUnpacker_Init(&xzu.p);

  if (!xzu.InBuf)
  {
    xzu.InBuf = (Byte *)MidAlloc(kInBufSize);
    if (!xzu.InBuf)
      return E_OUTOFMEMORY;
  }
  if (!xzu.OutBuf)
  {
    xzu.OutBuf = (Byte *)MidAlloc(kOutBufSize);
    if (!xzu.OutBuf)
      return E_OUTOFMEMORY;
  }
  
  UInt32 inSize = 0;
  UInt32 inPos = 0;
  SizeT outPos = 0;

  HRESULT readRes = S_OK;

  for (;;)
  {
    if (inPos == inSize && readRes == S_OK)
    {
      inPos = inSize = 0;
      readRes = seqInStream->Read(xzu.InBuf, kInBufSize, &inSize);
    }

    SizeT inLen = inSize - inPos;
    SizeT outLen = kOutBufSize - outPos;
    ECoderFinishMode finishMode = CODER_FINISH_ANY;

    /*
    // 17.01 : the code was disabled:
    if (inSize == 0)
      finishMode = CODER_FINISH_END;
    */

    if (outSizeLimit)
    {
      const UInt64 rem = *outSizeLimit - OutSize;
      if (outLen >= rem)
      {
        outLen = (SizeT)rem;
        if (finishStream)
          finishMode = CODER_FINISH_END;
      }
    }
    
    ECoderStatus status;

    const SizeT outLenRequested = outLen;

    SRes res = XzUnpacker_Code(&xzu.p,
        xzu.OutBuf + outPos, &outLen,
        xzu.InBuf + inPos, &inLen,
        finishMode, &status);

    DecodeRes = res;

    inPos += (UInt32)inLen;
    outPos += outLen;

    InSize += inLen;
    OutSize += outLen;

    bool finished = ((inLen == 0 && outLen == 0) || res != SZ_OK);

    if (outLen >= outLenRequested || finished)
    {
      if (outStream && outPos != 0)
      {
        RINOK(WriteStream(outStream, xzu.OutBuf, outPos));
      }
      outPos = 0;
    }
    
    if (progress)
    {
      RINOK(progress->SetRatioInfo(&InSize, &OutSize));
    }
    
    if (!finished)
      continue;

    {
      PhySize = InSize;
      NumStreams = xzu.p.numStartedStreams;
      if (NumStreams > 0)
        IsArc = true;
      NumBlocks = xzu.p.numTotalBlocks;

      UnpackSize_Defined = true;
      NumStreams_Defined = true;
      NumBlocks_Defined = true;

      UInt64 extraSize = XzUnpacker_GetExtraSize(&xzu.p);

      if (res == SZ_OK)
      {
        if (status == CODER_STATUS_NEEDS_MORE_INPUT)
        {
          extraSize = 0;
          if (!XzUnpacker_IsStreamWasFinished(&xzu.p))
          {
            // finished at padding bytes, but padding is not aligned for 4
            UnexpectedEnd = true;
            res = SZ_ERROR_DATA;
          }
        }
        else // status == CODER_STATUS_NOT_FINISHED
          res = SZ_ERROR_DATA;
      }
      else if (res == SZ_ERROR_NO_ARCHIVE)
      {
        if (InSize == extraSize)
          IsArc = false;
        else
        {
          if (extraSize != 0 || inPos != inSize)
          {
            DataAfterEnd = true;
            res = SZ_OK;
          }
        }
      }

      DecodeRes = res;
      PhySize -= extraSize;

      switch (res)
      {
        case SZ_OK: break;
        case SZ_ERROR_NO_ARCHIVE: IsArc = false; break;
        case SZ_ERROR_ARCHIVE: HeadersError = true; break;
        case SZ_ERROR_UNSUPPORTED: Unsupported = true; break;
        case SZ_ERROR_CRC: CrcError = true; break;
        case SZ_ERROR_DATA: DataError = true; break;
        default: DataError = true; break;
      }

      return readRes;
    }
  }
}


Int32 CDecoder::Get_Extract_OperationResult() const
{
  Int32 opRes;
  if (!IsArc)
    opRes = NExtract::NOperationResult::kIsNotArc;
  else if (UnexpectedEnd)
    opRes = NExtract::NOperationResult::kUnexpectedEnd;
  else if (DataAfterEnd)
    opRes = NExtract::NOperationResult::kDataAfterEnd;
  else if (CrcError)
    opRes = NExtract::NOperationResult::kCRCError;
  else if (Unsupported)
    opRes = NExtract::NOperationResult::kUnsupportedMethod;
  else if (HeadersError)
    opRes = NExtract::NOperationResult::kDataError;
  else if (DataError)
    opRes = NExtract::NOperationResult::kDataError;
  else if (DecodeRes != SZ_OK)
    opRes = NExtract::NOperationResult::kDataError;
  else
    opRes = NExtract::NOperationResult::kOK;
  return opRes;
}



HRESULT CComDecoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 * /* inSize */, const UInt64 *outSize, ICompressProgressInfo *progress)
{
  RINOK(_decoder.Decode(inStream, outStream, outSize, _finishStream, progress));
  Int32 opRes = _decoder.Get_Extract_OperationResult();
  if (opRes == NArchive::NExtract::NOperationResult::kUnsupportedMethod)
    return E_NOTIMPL;
  if (opRes != NArchive::NExtract::NOperationResult::kOK)
    return S_FALSE;
  return S_OK;
}

STDMETHODIMP CComDecoder::SetFinishMode(UInt32 finishMode)
{
  _finishStream = (finishMode != 0);
  return S_OK;
}

STDMETHODIMP CComDecoder::GetInStreamProcessedSize(UInt64 *value)
{
  *value = _decoder.InSize;
  return S_OK;
}

}}
