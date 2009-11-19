// MultiStream.cpp

#include "StdAfx.h"

#include "MultiStream.h"

STDMETHODIMP CMultiStream::Read(void *data, UInt32 size, UInt32 *processedSize)
{
  if(processedSize != NULL)
    *processedSize = 0;
  while(_streamIndex < Streams.Size() && size > 0)
  {
    CSubStreamInfo &s = Streams[_streamIndex];
    if (_pos == s.Size)
    {
      _streamIndex++;
      _pos = 0;
      continue;
    }
    RINOK(s.Stream->Seek(s.Pos + _pos, STREAM_SEEK_SET, 0));
    UInt32 sizeToRead = UInt32(MyMin((UInt64)size, s.Size - _pos));
    UInt32 realProcessed;
    HRESULT result = s.Stream->Read(data, sizeToRead, &realProcessed);
    data = (void *)((Byte *)data + realProcessed);
    size -= realProcessed;
    if(processedSize != NULL)
      *processedSize += realProcessed;
    _pos += realProcessed;
    _seekPos += realProcessed;
    RINOK(result);
    break;
  }
  return S_OK;
}
  
STDMETHODIMP CMultiStream::Seek(Int64 offset, UInt32 seekOrigin,
    UInt64 *newPosition)
{
  UInt64 newPos;
  switch(seekOrigin)
  {
    case STREAM_SEEK_SET:
      newPos = offset;
      break;
    case STREAM_SEEK_CUR:
      newPos = _seekPos + offset;
      break;
    case STREAM_SEEK_END:
      newPos = _totalLength + offset;
      break;
    default:
      return STG_E_INVALIDFUNCTION;
  }
  _seekPos = 0;
  for (_streamIndex = 0; _streamIndex < Streams.Size(); _streamIndex++)
  {
    UInt64 size = Streams[_streamIndex].Size;
    if (newPos < _seekPos + size)
    {
      _pos = newPos - _seekPos;
      _seekPos += _pos;
      if (newPosition != 0)
        *newPosition = newPos;
      return S_OK;
    }
    _seekPos += size;
  }
  if (newPos == _seekPos)
  {
    if (newPosition != 0)
      *newPosition = newPos;
    return S_OK;
  }
  return E_FAIL;
}


/*
class COutVolumeStream:
  public ISequentialOutStream,
  public CMyUnknownImp
{
  int _volIndex;
  UInt64 _volSize;
  UInt64 _curPos;
  CMyComPtr<ISequentialOutStream> _volumeStream;
  COutArchive _archive;
  CCRC _crc;

public:
  MY_UNKNOWN_IMP

  CFileItem _file;
  CUpdateOptions _options;
  CMyComPtr<IArchiveUpdateCallback2> VolumeCallback;
  void Init(IArchiveUpdateCallback2 *volumeCallback,
      const UString &name)
  {
    _file.Name = name;
    _file.IsStartPosDefined = true;
    _file.StartPos = 0;
    
    VolumeCallback = volumeCallback;
    _volIndex = 0;
    _volSize = 0;
  }
  
  HRESULT Flush();
  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
};

HRESULT COutVolumeStream::Flush()
{
  if (_volumeStream)
  {
    _file.UnPackSize = _curPos;
    _file.FileCRC = _crc.GetDigest();
    RINOK(WriteVolumeHeader(_archive, _file, _options));
    _archive.Close();
    _volumeStream.Release();
    _file.StartPos += _file.UnPackSize;
  }
  return S_OK;
}
*/

/*
STDMETHODIMP COutMultiStream::Write(const void *data, UInt32 size, UInt32 *processedSize)
{
  if(processedSize != NULL)
    *processedSize = 0;
  while(size > 0)
  {
    if (_streamIndex >= Streams.Size())
    {
      CSubStreamInfo subStream;
      RINOK(VolumeCallback->GetVolumeSize(Streams.Size(), &subStream.Size));
      RINOK(VolumeCallback->GetVolumeStream(Streams.Size(), &subStream.Stream));
      subStream.Pos = 0;
      Streams.Add(subStream);
      continue;
    }
    CSubStreamInfo &subStream = Streams[_streamIndex];
    if (_offsetPos >= subStream.Size)
    {
      _offsetPos -= subStream.Size;
      _streamIndex++;
      continue;
    }
    if (_offsetPos != subStream.Pos)
    {
      CMyComPtr<IOutStream> outStream;
      RINOK(subStream.Stream.QueryInterface(IID_IOutStream, &outStream));
      RINOK(outStream->Seek(_offsetPos, STREAM_SEEK_SET, NULL));
      subStream.Pos = _offsetPos;
    }

    UInt32 curSize = (UInt32)MyMin((UInt64)size, subStream.Size - subStream.Pos);
    UInt32 realProcessed;
    RINOK(subStream.Stream->Write(data, curSize, &realProcessed));
    data = (void *)((Byte *)data + realProcessed);
    size -= realProcessed;
    subStream.Pos += realProcessed;
    _offsetPos += realProcessed;
    _absPos += realProcessed;
    if (_absPos > _length)
      _length = _absPos;
    if(processedSize != NULL)
      *processedSize += realProcessed;
    if (subStream.Pos == subStream.Size)
    {
      _streamIndex++;
      _offsetPos = 0;
    }
    if (realProcessed != curSize && realProcessed == 0)
      return E_FAIL;
  }
  return S_OK;
}

STDMETHODIMP COutMultiStream::Seek(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition)
{
  if(seekOrigin >= 3)
    return STG_E_INVALIDFUNCTION;
  switch(seekOrigin)
  {
    case STREAM_SEEK_SET:
      _absPos = offset;
      break;
    case STREAM_SEEK_CUR:
      _absPos += offset;
      break;
    case STREAM_SEEK_END:
      _absPos = _length + offset;
      break;
  }
  _offsetPos = _absPos;
  _streamIndex = 0;
  return S_OK;
}
*/
