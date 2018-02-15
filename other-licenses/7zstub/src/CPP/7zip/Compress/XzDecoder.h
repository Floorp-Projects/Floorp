// XzDecoder.h

#ifndef __XZ_DECODER_H
#define __XZ_DECODER_H

#include "../../../C/Xz.h"

#include "../../Common/MyCom.h"

#include "../ICoder.h"

namespace NCompress {
namespace NXz {

struct CXzUnpackerCPP
{
  Byte *InBuf;
  Byte *OutBuf;
  CXzUnpacker p;
  
  CXzUnpackerCPP();
  ~CXzUnpackerCPP();
};


struct CStatInfo
{
  UInt64 InSize;
  UInt64 OutSize;
  UInt64 PhySize;

  UInt64 NumStreams;
  UInt64 NumBlocks;

  bool UnpackSize_Defined;

  bool NumStreams_Defined;
  bool NumBlocks_Defined;

  bool IsArc;
  bool UnexpectedEnd;
  bool DataAfterEnd;
  bool Unsupported;
  bool HeadersError;
  bool DataError;
  bool CrcError;

  CStatInfo() { Clear(); }

  void Clear();
};


struct CDecoder: public CStatInfo
{
  CXzUnpackerCPP xzu;
  SRes DecodeRes; // it's not HRESULT

  CDecoder(): DecodeRes(SZ_OK) {}

  /* Decode() can return ERROR code only if there is progress or stream error.
     Decode() returns S_OK in case of xz decoding error, but DecodeRes and CStatInfo contain error information */
  HRESULT Decode(ISequentialInStream *seqInStream, ISequentialOutStream *outStream,
      const UInt64 *outSizeLimit, bool finishStream, ICompressProgressInfo *compressProgress);
  Int32 Get_Extract_OperationResult() const;
};


class CComDecoder:
  public ICompressCoder,
  public ICompressSetFinishMode,
  public ICompressGetInStreamProcessedSize,
  public CMyUnknownImp
{
  CDecoder _decoder;
  bool _finishStream;

public:
  MY_UNKNOWN_IMP2(
      ICompressSetFinishMode,
      ICompressGetInStreamProcessedSize)
  
  STDMETHOD(Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);
  STDMETHOD(SetFinishMode)(UInt32 finishMode);
  STDMETHOD(GetInStreamProcessedSize)(UInt64 *value);

  CComDecoder(): _finishStream(false) {}
};

}}

#endif
