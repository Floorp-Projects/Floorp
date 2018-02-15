// MemBlocks.h

#ifndef __MEM_BLOCKS_H
#define __MEM_BLOCKS_H

#include "../../Common/MyVector.h"

#include "../../Windows/Synchronization.h"

#include "../IStream.h"

class CMemBlockManager
{
  void *_data;
  size_t _blockSize;
  void *_headFree;
public:
  CMemBlockManager(size_t blockSize = (1 << 20)): _data(0), _blockSize(blockSize), _headFree(0) {}
  ~CMemBlockManager() { FreeSpace(); }

  bool AllocateSpace(size_t numBlocks);
  void FreeSpace();
  size_t GetBlockSize() const { return _blockSize; }
  void *AllocateBlock();
  void FreeBlock(void *p);
};


class CMemBlockManagerMt: public CMemBlockManager
{
  NWindows::NSynchronization::CCriticalSection _criticalSection;
public:
  NWindows::NSynchronization::CSemaphore Semaphore;

  CMemBlockManagerMt(size_t blockSize = (1 << 20)): CMemBlockManager(blockSize) {}
  ~CMemBlockManagerMt() { FreeSpace(); }

  HRes AllocateSpace(size_t numBlocks, size_t numNoLockBlocks = 0);
  HRes AllocateSpaceAlways(size_t desiredNumberOfBlocks, size_t numNoLockBlocks = 0);
  void FreeSpace();
  void *AllocateBlock();
  void FreeBlock(void *p, bool lockMode = true);
  HRes ReleaseLockedBlocks(int number) { return Semaphore.Release(number); }
};


class CMemBlocks
{
  void Free(CMemBlockManagerMt *manager);
public:
  CRecordVector<void *> Blocks;
  UInt64 TotalSize;
  
  CMemBlocks(): TotalSize(0) {}

  void FreeOpt(CMemBlockManagerMt *manager);
  HRESULT WriteToStream(size_t blockSize, ISequentialOutStream *outStream) const;
};

struct CMemLockBlocks: public CMemBlocks
{
  bool LockMode;

  CMemLockBlocks(): LockMode(true) {};
  void Free(CMemBlockManagerMt *memManager);
  void FreeBlock(int index, CMemBlockManagerMt *memManager);
  HRes SwitchToNoLockMode(CMemBlockManagerMt *memManager);
  void Detach(CMemLockBlocks &blocks, CMemBlockManagerMt *memManager);
};

#endif
