// Lzma2Decoder.h

#ifndef __LZMA2_DECODER_H
#define __LZMA2_DECODER_H

#include "../../../C/Lzma2Dec.h"

#include "../../Common/MyCom.h"
#include "../ICoder.h"

namespace NCompress {
namespace NLzma2 {

class CDecoder:
  public ICompressCoder,
  public ICompressSetDecoderProperties2,
  public ICompressSetFinishMode,
  public ICompressGetInStreamProcessedSize,
  public ICompressSetBufSize,
  #ifndef NO_READ_FROM_CODER
  public ICompressSetInStream,
  public ICompressSetOutStreamSize,
  public ISequentialInStream,
  #endif
  public CMyUnknownImp
{
  Byte *_inBuf;
  UInt32 _inPos;
  UInt32 _inLim;

  bool _finishMode;
  bool _outSizeDefined;
  UInt64 _outSize;
  UInt64 _inProcessed;
  UInt64 _outProcessed;
  
  UInt32 _outStep;
  UInt32 _inBufSize;
  UInt32 _inBufSizeNew;

  CLzma2Dec _state;

public:
  MY_QUERYINTERFACE_BEGIN2(ICompressCoder)
  MY_QUERYINTERFACE_ENTRY(ICompressSetDecoderProperties2)
  MY_QUERYINTERFACE_ENTRY(ICompressSetFinishMode)
  MY_QUERYINTERFACE_ENTRY(ICompressGetInStreamProcessedSize)
  MY_QUERYINTERFACE_ENTRY(ICompressSetBufSize)
  #ifndef NO_READ_FROM_CODER
  MY_QUERYINTERFACE_ENTRY(ICompressSetInStream)
  MY_QUERYINTERFACE_ENTRY(ICompressSetOutStreamSize)
  MY_QUERYINTERFACE_ENTRY(ISequentialInStream)
  #endif
  MY_QUERYINTERFACE_END
  MY_ADDREF_RELEASE

  STDMETHOD(Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);
  STDMETHOD(SetDecoderProperties2)(const Byte *data, UInt32 size);
  STDMETHOD(SetFinishMode)(UInt32 finishMode);
  STDMETHOD(GetInStreamProcessedSize)(UInt64 *value);
  STDMETHOD(SetOutStreamSize)(const UInt64 *outSize);
  STDMETHOD(SetInBufSize)(UInt32 streamIndex, UInt32 size);
  STDMETHOD(SetOutBufSize)(UInt32 streamIndex, UInt32 size);

  #ifndef NO_READ_FROM_CODER

private:
  CMyComPtr<ISequentialInStream> _inStream;
public:

  STDMETHOD(SetInStream)(ISequentialInStream *inStream);
  STDMETHOD(ReleaseInStream)();
  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);

  #endif

  CDecoder();
  virtual ~CDecoder();
};

}}

#endif
