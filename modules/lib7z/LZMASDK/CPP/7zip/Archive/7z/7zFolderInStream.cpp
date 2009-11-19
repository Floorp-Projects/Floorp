// 7zFolderInStream.cpp

#include "StdAfx.h"

#include "7zFolderInStream.h"

namespace NArchive {
namespace N7z {

CFolderInStream::CFolderInStream()
{
  _inStreamWithHashSpec = new CSequentialInStreamWithCRC;
  _inStreamWithHash = _inStreamWithHashSpec;
}

void CFolderInStream::Init(IArchiveUpdateCallback *updateCallback,
    const UInt32 *fileIndices, UInt32 numFiles)
{
  _updateCallback = updateCallback;
  _numFiles = numFiles;
  _fileIndex = 0;
  _fileIndices = fileIndices;
  Processed.Clear();
  CRCs.Clear();
  Sizes.Clear();
  _fileIsOpen = false;
  _currentSizeIsDefined = false;
}

HRESULT CFolderInStream::OpenStream()
{
  _filePos = 0;
  while (_fileIndex < _numFiles)
  {
    _currentSizeIsDefined = false;
    CMyComPtr<ISequentialInStream> stream;
    HRESULT result = _updateCallback->GetStream(_fileIndices[_fileIndex], &stream);
    if (result != S_OK && result != S_FALSE)
      return result;
    _fileIndex++;
    _inStreamWithHashSpec->SetStream(stream);
    _inStreamWithHashSpec->Init();
    if (!stream)
    {
      RINOK(_updateCallback->SetOperationResult(NArchive::NUpdate::NOperationResult::kOK));
      Sizes.Add(0);
      Processed.Add(result == S_OK);
      AddDigest();
      continue;
    }
    CMyComPtr<IStreamGetSize> streamGetSize;
    if (stream.QueryInterface(IID_IStreamGetSize, &streamGetSize) == S_OK)
    {
      if(streamGetSize)
      {
        _currentSizeIsDefined = true;
        RINOK(streamGetSize->GetSize(&_currentSize));
      }
    }

    _fileIsOpen = true;
    return S_OK;
  }
  return S_OK;
}

void CFolderInStream::AddDigest()
{
  CRCs.Add(_inStreamWithHashSpec->GetCRC());
}

HRESULT CFolderInStream::CloseStream()
{
  RINOK(_updateCallback->SetOperationResult(NArchive::NUpdate::NOperationResult::kOK));
  _inStreamWithHashSpec->ReleaseStream();
  _fileIsOpen = false;
  Processed.Add(true);
  Sizes.Add(_filePos);
  AddDigest();
  return S_OK;
}

STDMETHODIMP CFolderInStream::Read(void *data, UInt32 size, UInt32 *processedSize)
{
  UInt32 realProcessedSize = 0;
  while ((_fileIndex < _numFiles || _fileIsOpen) && size > 0)
  {
    if (_fileIsOpen)
    {
      UInt32 localProcessedSize;
      RINOK(_inStreamWithHash->Read(
          ((Byte *)data) + realProcessedSize, size, &localProcessedSize));
      if (localProcessedSize == 0)
      {
        RINOK(CloseStream());
        continue;
      }
      realProcessedSize += localProcessedSize;
      _filePos += localProcessedSize;
      size -= localProcessedSize;
      break;
    }
    else
    {
      RINOK(OpenStream());
    }
  }
  if (processedSize != 0)
    *processedSize = realProcessedSize;
  return S_OK;
}

STDMETHODIMP CFolderInStream::GetSubStreamSize(UInt64 subStream, UInt64 *value)
{
  *value = 0;
  int subStreamIndex = (int)subStream;
  if (subStreamIndex < 0 || subStream > Sizes.Size())
    return E_FAIL;
  if (subStreamIndex < Sizes.Size())
  {
    *value= Sizes[subStreamIndex];
    return S_OK;
  }
  if (!_currentSizeIsDefined)
    return S_FALSE;
  *value = _currentSize;
  return S_OK;
}

}}
