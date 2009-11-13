// LzmaDecoder.h

#ifndef __LZMA_DECODER_H
#define __LZMA_DECODER_H

#include "../../../C/LzmaDec.h"

#include "../../Common/MyCom.h"
#include "../ICoder.h"

namespace NCompress {
namespace NLzma {

class CDecoder:
  public ICompressCoder,
  public ICompressSetDecoderProperties2,
  #ifndef NO_READ_FROM_CODER
  public ICompressSetInStream,
  public ICompressSetOutStreamSize,
  public ISequentialInStream,
  #endif
  public CMyUnknownImp
{
  CMyComPtr<ISequentialInStream> _inStream;
  Byte *_inBuf;
  UInt32 _inPos;
  UInt32 _inSize;
  CLzmaDec _state;
  bool _propsWereSet;
  bool _outSizeDefined;
  UInt64 _outSize;
  UInt64 _inSizeProcessed;
  UInt64 _outSizeProcessed;
  
  HRESULT CreateInputBuffer();
  HRESULT CodeSpec(ISequentialInStream *inStream, ISequentialOutStream *outStream, ICompressProgressInfo *progress);
  void SetOutStreamSizeResume(const UInt64 *outSize);

public:
  MY_QUERYINTERFACE_BEGIN
  MY_QUERYINTERFACE_ENTRY(ICompressSetDecoderProperties2)
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
  STDMETHOD(SetOutStreamSize)(const UInt64 *outSize);

  #ifndef NO_READ_FROM_CODER
  
  STDMETHOD(SetInStream)(ISequentialInStream *inStream);
  STDMETHOD(ReleaseInStream)();
  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);

  HRESULT CodeResume(ISequentialOutStream *outStream, const UInt64 *outSize, ICompressProgressInfo *progress);
  HRESULT ReadFromInputStream(void *data, UInt32 size, UInt32 *processedSize);
  UInt64 GetInputProcessedSize() const { return _inSizeProcessed; }

  #endif

  bool FinishStream;

  CDecoder();
  virtual ~CDecoder();

};

}}

#endif
