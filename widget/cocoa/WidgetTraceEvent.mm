/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <Cocoa/Cocoa.h>
#include "CustomCocoaEvents.h"
#include <Foundation/NSAutoreleasePool.h>
#include <mozilla/CondVar.h>
#include <mozilla/Mutex.h>
#include "mozilla/WidgetTraceEvent.h"

using mozilla::CondVar;
using mozilla::Mutex;
using mozilla::MutexAutoLock;

namespace {

Mutex* sMutex = NULL;
CondVar* sCondVar = NULL;
bool sTracerProcessed = false;

}

namespace mozilla {

bool InitWidgetTracing()
{
  sMutex = new Mutex("Event tracer thread mutex");
  sCondVar = new CondVar(*sMutex, "Event tracer thread condvar");
  return sMutex && sCondVar;
}

void CleanUpWidgetTracing()
{
  delete sMutex;
  delete sCondVar;
  sMutex = NULL;
  sCondVar = NULL;
}

// This function is called from the main (UI) thread.
void SignalTracerThread()
{
  if (!sMutex || !sCondVar)
    return;
  MutexAutoLock lock(*sMutex);
  NS_ABORT_IF_FALSE(!sTracerProcessed, "Tracer synchronization state is wrong");
  sTracerProcessed = true;
  sCondVar->Notify();
}

// This function is called from the background tracer thread.
bool FireAndWaitForTracerEvent()
{
  NS_ABORT_IF_FALSE(sMutex && sCondVar, "Tracing not initialized!");
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
  MutexAutoLock lock(*sMutex);
  NS_ABORT_IF_FALSE(!sTracerProcessed, "Tracer synchronization state is wrong");

  // Post an application-defined event to the main thread's event queue
  // and wait for it to get processed.
  [NSApp postEvent:[NSEvent otherEventWithType:NSApplicationDefined
                                      location:NSMakePoint(0,0)
                                 modifierFlags:0
                                     timestamp:0
                                  windowNumber:0
                                       context:NULL
                                       subtype:kEventSubtypeTrace
                                         data1:0
                                         data2:0]
             atStart:NO];
  while (!sTracerProcessed)
    sCondVar->Wait();
  sTracerProcessed = false;
  [pool release];
  return true;
}

}  // namespace mozilla
