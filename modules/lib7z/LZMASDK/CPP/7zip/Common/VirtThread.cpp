// VirtThread.cpp

#include "StdAfx.h"

#include "VirtThread.h"

static THREAD_FUNC_DECL CoderThread(void *p)
{
  for (;;)
  {
    CVirtThread *t = (CVirtThread *)p;
    t->StartEvent.Lock();
    if (t->ExitEvent)
      return 0;
    t->Execute();
    t->FinishedEvent.Set();
  }
}

WRes CVirtThread::Create()
{
  RINOK(StartEvent.CreateIfNotCreated());
  RINOK(FinishedEvent.CreateIfNotCreated());
  StartEvent.Reset();
  FinishedEvent.Reset();
  ExitEvent = false;
  if (Thread.IsCreated())
    return S_OK;
  return Thread.Create(CoderThread, this);
}

void CVirtThread::Start()
{
  ExitEvent = false;
  StartEvent.Set();
}

CVirtThread::~CVirtThread()
{
  ExitEvent = true;
  if (StartEvent.IsCreated())
    StartEvent.Set();
  if (Thread.IsCreated())
    Thread.Wait();
}

