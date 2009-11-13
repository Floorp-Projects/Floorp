// MultiStream.h

#ifndef __MULTISTREAM_H
#define __MULTISTREAM_H

#include "../../../Common/MyCom.h"
#include "../../../Common/MyVector.h"
#include "../../Archive/IArchive.h"

class CMultiStream:
  public IInStream,
  public CMyUnknownImp
{
  int _streamIndex;
  UInt64 _pos;
  UInt64 _seekPos;
  UInt64 _totalLength;
public:
  struct CSubStreamInfo
  {
    CMyComPtr<IInStream> Stream;
    UInt64 Pos;
    UInt64 Size;
  };
  CObjectVector<CSubStreamInfo> Streams;
  void Init()
  {
    _streamIndex = 0;
    _pos = 0;
    _seekPos = 0;
    _totalLength = 0;
    for (int i = 0; i < Streams.Size(); i++)
      _totalLength += Streams[i].Size;
  }

  MY_UNKNOWN_IMP1(IInStream)

  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition);
};

/*
class COutMultiStream:
  public IOutStream,
  public CMyUnknownImp
{
  int _streamIndex; // required stream
  UInt64 _offsetPos; // offset from start of _streamIndex index
  UInt64 _absPos;
  UInt64 _length;

  struct CSubStreamInfo
  {
    CMyComPtr<ISequentialOutStream> Stream;
    UInt64 Size;
    UInt64 Pos;
 };
  CObjectVector<CSubStreamInfo> Streams;
public:
  CMyComPtr<IArchiveUpdateCallback2> VolumeCallback;
  void Init()
  {
    _streamIndex = 0;
    _offsetPos = 0;
    _absPos = 0;
    _length = 0;
  }

  MY_UNKNOWN_IMP1(IOutStream)

  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition);
};
*/

#endif
