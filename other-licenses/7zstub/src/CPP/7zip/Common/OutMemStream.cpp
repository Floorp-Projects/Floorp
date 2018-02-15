// OutMemStream.cpp

#include "StdAfx.h"

#include "OutMemStream.h"

void COutMemStream::Free()
{
  Blocks.Free(_memManager);
  Blocks.LockMode = true;
}

void COutMemStream::Init()
{
  WriteToRealStreamEvent.Reset();
  _unlockEventWasSent = false;
  _realStreamMode = false;
  Free();
  _curBlockPos = 0;
  _curBlockIndex = 0;
}

void COutMemStream::DetachData(CMemLockBlocks &blocks)
{
  Blocks.Detach(blocks, _memManager);
  Free();
}


HRESULT COutMemStream::WriteToRealStream()
{
  RINOK(Blocks.WriteToStream(_memManager->GetBlockSize(), OutSeqStream));
  Blocks.Free(_memManager);
  return S_OK;
}

STDMETHODIMP COutMemStream::Write(const void *data, UInt32 size, UInt32 *processedSize)
{
  if (_realStreamMode)
    return OutSeqStream->Write(data, size, processedSize);
  if (processedSize != 0)
    *processedSize = 0;
  while (size != 0)
  {
    if (_curBlockIndex < Blocks.Blocks.Size())
    {
      Byte *p = (Byte *)Blocks.Blocks[_curBlockIndex] + _curBlockPos;
      size_t curSize = _memManager->GetBlockSize() - _curBlockPos;
      if (size < curSize)
        curSize = size;
      memcpy(p, data, curSize);
      if (processedSize != 0)
        *processedSize += (UInt32)curSize;
      data = (const void *)((const Byte *)data + curSize);
      size -= (UInt32)curSize;
      _curBlockPos += curSize;

      UInt64 pos64 = GetPos();
      if (pos64 > Blocks.TotalSize)
        Blocks.TotalSize = pos64;
      if (_curBlockPos == _memManager->GetBlockSize())
      {
        _curBlockIndex++;
        _curBlockPos = 0;
      }
      continue;
    }
    HANDLE events[3] = { StopWritingEvent, WriteToRealStreamEvent, /* NoLockEvent, */ _memManager->Semaphore };
    DWORD waitResult = ::WaitForMultipleObjects((Blocks.LockMode ? 3 : 2), events, FALSE, INFINITE);
    switch (waitResult)
    {
      case (WAIT_OBJECT_0 + 0):
        return StopWriteResult;
      case (WAIT_OBJECT_0 + 1):
      {
        _realStreamMode = true;
        RINOK(WriteToRealStream());
        UInt32 processedSize2;
        HRESULT res = OutSeqStream->Write(data, size, &processedSize2);
        if (processedSize != 0)
          *processedSize += processedSize2;
        return res;
      }
      /*
      case (WAIT_OBJECT_0 + 2):
      {
        // it has bug: no write.
        if (!Blocks.SwitchToNoLockMode(_memManager))
          return E_FAIL;
        break;
      }
      */
      case (WAIT_OBJECT_0 + 2):
        break;
      default:
        return E_FAIL;
    }
    Blocks.Blocks.Add(_memManager->AllocateBlock());
    if (Blocks.Blocks.Back() == 0)
      return E_FAIL;
  }
  return S_OK;
}

STDMETHODIMP COutMemStream::Seek(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition)
{
  if (_realStreamMode)
  {
    if (!OutStream)
      return E_FAIL;
    return OutStream->Seek(offset, seekOrigin, newPosition);
  }
  if (seekOrigin == STREAM_SEEK_CUR)
  {
    if (offset != 0)
      return E_NOTIMPL;
  }
  else if (seekOrigin == STREAM_SEEK_SET)
  {
    if (offset != 0)
      return E_NOTIMPL;
    _curBlockIndex = 0;
    _curBlockPos = 0;
  }
  else
    return E_NOTIMPL;
  if (newPosition)
    *newPosition = GetPos();
  return S_OK;
}

STDMETHODIMP COutMemStream::SetSize(UInt64 newSize)
{
  if (_realStreamMode)
  {
    if (!OutStream)
      return E_FAIL;
    return OutStream->SetSize(newSize);
  }
  Blocks.TotalSize = newSize;
  return S_OK;
}
