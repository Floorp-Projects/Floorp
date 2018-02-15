// MemBlocks.cpp

#include "StdAfx.h"

#include "../../../C/Alloc.h"

#include "MemBlocks.h"
#include "StreamUtils.h"

bool CMemBlockManager::AllocateSpace(size_t numBlocks)
{
  FreeSpace();
  if (_blockSize < sizeof(void *) || numBlocks < 1)
    return false;
  size_t totalSize = numBlocks * _blockSize;
  if (totalSize / _blockSize != numBlocks)
    return false;
  _data = ::MidAlloc(totalSize);
  if (_data == 0)
    return false;
  Byte *p = (Byte *)_data;
  for (size_t i = 0; i + 1 < numBlocks; i++, p += _blockSize)
    *(Byte **)p = (p + _blockSize);
  *(Byte **)p = 0;
  _headFree = _data;
  return true;
}

void CMemBlockManager::FreeSpace()
{
  ::MidFree(_data);
  _data = 0;
  _headFree= 0;
}

void *CMemBlockManager::AllocateBlock()
{
  if (_headFree == 0)
    return 0;
  void *p = _headFree;
  _headFree = *(void **)_headFree;
  return p;
}

void CMemBlockManager::FreeBlock(void *p)
{
  if (p == 0)
    return;
  *(void **)p = _headFree;
  _headFree = p;
}


HRes CMemBlockManagerMt::AllocateSpace(size_t numBlocks, size_t numNoLockBlocks)
{
  if (numNoLockBlocks > numBlocks)
    return E_INVALIDARG;
  if (!CMemBlockManager::AllocateSpace(numBlocks))
    return E_OUTOFMEMORY;
  size_t numLockBlocks = numBlocks - numNoLockBlocks;
  Semaphore.Close();
  return Semaphore.Create((LONG)numLockBlocks, (LONG)numLockBlocks);
}

HRes CMemBlockManagerMt::AllocateSpaceAlways(size_t desiredNumberOfBlocks, size_t numNoLockBlocks)
{
  if (numNoLockBlocks > desiredNumberOfBlocks)
    return E_INVALIDARG;
  for (;;)
  {
    if (AllocateSpace(desiredNumberOfBlocks, numNoLockBlocks) == 0)
      return 0;
    if (desiredNumberOfBlocks == numNoLockBlocks)
      return E_OUTOFMEMORY;
    desiredNumberOfBlocks = numNoLockBlocks + ((desiredNumberOfBlocks - numNoLockBlocks) >> 1);
  }
}

void CMemBlockManagerMt::FreeSpace()
{
  Semaphore.Close();
  CMemBlockManager::FreeSpace();
}

void *CMemBlockManagerMt::AllocateBlock()
{
  // Semaphore.Lock();
  NWindows::NSynchronization::CCriticalSectionLock lock(_criticalSection);
  return CMemBlockManager::AllocateBlock();
}

void CMemBlockManagerMt::FreeBlock(void *p, bool lockMode)
{
  if (p == 0)
    return;
  {
    NWindows::NSynchronization::CCriticalSectionLock lock(_criticalSection);
    CMemBlockManager::FreeBlock(p);
  }
  if (lockMode)
    Semaphore.Release();
}

void CMemBlocks::Free(CMemBlockManagerMt *manager)
{
  while (Blocks.Size() > 0)
  {
    manager->FreeBlock(Blocks.Back());
    Blocks.DeleteBack();
  }
  TotalSize = 0;
}

void CMemBlocks::FreeOpt(CMemBlockManagerMt *manager)
{
  Free(manager);
  Blocks.ClearAndFree();
}

HRESULT CMemBlocks::WriteToStream(size_t blockSize, ISequentialOutStream *outStream) const
{
  UInt64 totalSize = TotalSize;
  for (unsigned blockIndex = 0; totalSize > 0; blockIndex++)
  {
    UInt32 curSize = (UInt32)blockSize;
    if (totalSize < curSize)
      curSize = (UInt32)totalSize;
    if (blockIndex >= Blocks.Size())
      return E_FAIL;
    RINOK(WriteStream(outStream, Blocks[blockIndex], curSize));
    totalSize -= curSize;
  }
  return S_OK;
}


void CMemLockBlocks::FreeBlock(int index, CMemBlockManagerMt *memManager)
{
  memManager->FreeBlock(Blocks[index], LockMode);
  Blocks[index] = 0;
}

void CMemLockBlocks::Free(CMemBlockManagerMt *memManager)
{
  while (Blocks.Size() > 0)
  {
    FreeBlock(Blocks.Size() - 1, memManager);
    Blocks.DeleteBack();
  }
  TotalSize = 0;
}

HRes CMemLockBlocks::SwitchToNoLockMode(CMemBlockManagerMt *memManager)
{
  if (LockMode)
  {
    if (Blocks.Size() > 0)
    {
      RINOK(memManager->ReleaseLockedBlocks(Blocks.Size()));
    }
    LockMode = false;
  }
  return 0;
}

void CMemLockBlocks::Detach(CMemLockBlocks &blocks, CMemBlockManagerMt *memManager)
{
  blocks.Free(memManager);
  blocks.LockMode = LockMode;
  UInt64 totalSize = 0;
  size_t blockSize = memManager->GetBlockSize();
  FOR_VECTOR (i, Blocks)
  {
    if (totalSize < TotalSize)
      blocks.Blocks.Add(Blocks[i]);
    else
      FreeBlock(i, memManager);
    Blocks[i] = 0;
    totalSize += blockSize;
  }
  blocks.TotalSize = TotalSize;
  Free(memManager);
}
