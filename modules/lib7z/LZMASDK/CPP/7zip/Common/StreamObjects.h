// StreamObjects.h

#ifndef __STREAM_OBJECTS_H
#define __STREAM_OBJECTS_H

#include "../../Common/DynamicBuffer.h"
#include "../../Common/MyCom.h"
#include "../IStream.h"

struct CReferenceBuf:
  public IUnknown,
  public CMyUnknownImp
{
  CByteBuffer Buf;
  MY_UNKNOWN_IMP
};

class CBufInStream:
  public IInStream,
  public CMyUnknownImp
{
  const Byte *_data;
  UInt64 _pos;
  size_t _size;
  CMyComPtr<IUnknown> _ref;
public:
  void Init(const Byte *data, size_t size, IUnknown *ref = 0)
  {
    _data = data;
    _size = size;
    _pos = 0;
    _ref = ref;
  }
  void Init(CReferenceBuf *ref) { Init(ref->Buf, ref->Buf.GetCapacity(), ref); }

  MY_UNKNOWN_IMP1(IInStream)

  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition);
};

class CWriteBuffer
{
  CByteDynamicBuffer _buffer;
  size_t _size;
public:
  CWriteBuffer(): _size(0) {}
  void Init() { _size = 0;  }
  void Write(const void *data, size_t size);
  size_t GetSize() const { return _size; }
  const CByteDynamicBuffer& GetBuffer() const { return _buffer; }
};

class CSequentialOutStreamImp:
  public ISequentialOutStream,
  public CMyUnknownImp
{
  CWriteBuffer _writeBuffer;
public:
  void Init() { _writeBuffer.Init(); }
  size_t GetSize() const { return _writeBuffer.GetSize(); }
  const CByteDynamicBuffer& GetBuffer() const { return _writeBuffer.GetBuffer(); }

  MY_UNKNOWN_IMP

  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
};

class CSequentialOutStreamImp2:
  public ISequentialOutStream,
  public CMyUnknownImp
{
  Byte *_buffer;
  size_t _size;
  size_t _pos;
public:

  void Init(Byte *buffer, size_t size)
  {
    _buffer = buffer;
    _pos = 0;
    _size = size;
  }

  size_t GetPos() const { return _pos; }

  MY_UNKNOWN_IMP

  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
};

class CSequentialInStreamSizeCount:
  public ISequentialInStream,
  public CMyUnknownImp
{
  CMyComPtr<ISequentialInStream> _stream;
  UInt64 _size;
public:
  void Init(ISequentialInStream *stream)
  {
    _stream = stream;
    _size = 0;
  }
  UInt64 GetSize() const { return _size; }

  MY_UNKNOWN_IMP

  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
};

class CSequentialOutStreamSizeCount:
  public ISequentialOutStream,
  public CMyUnknownImp
{
  CMyComPtr<ISequentialOutStream> _stream;
  UInt64 _size;
public:
  void SetStream(ISequentialOutStream *stream) { _stream = stream; }
  void Init() { _size = 0; }
  UInt64 GetSize() const { return _size; }

  MY_UNKNOWN_IMP

  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
};

#endif
