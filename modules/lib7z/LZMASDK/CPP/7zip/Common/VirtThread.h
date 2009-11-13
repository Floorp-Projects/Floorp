// VirtThread.h

#ifndef __VIRTTHREAD_H
#define __VIRTTHREAD_H

#include "../../Windows/Synchronization.h"
#include "../../Windows/Thread.h"

struct CVirtThread
{
  NWindows::NSynchronization::CAutoResetEvent StartEvent;
  NWindows::NSynchronization::CAutoResetEvent FinishedEvent;
  NWindows::CThread Thread;
  bool ExitEvent;

  ~CVirtThread();
  WRes Create();
  void Start();
  void WaitFinish() { FinishedEvent.Lock(); }
  virtual void Execute() = 0;
};

#endif
