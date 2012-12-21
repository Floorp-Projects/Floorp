/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/WidgetTraceEvent.h"

#include <glib.h>
#include <mozilla/CondVar.h>
#include <mozilla/Mutex.h>
#include <stdio.h>

using mozilla::CondVar;
using mozilla::Mutex;
using mozilla::MutexAutoLock;

namespace {

Mutex* sMutex = NULL;
CondVar* sCondVar = NULL;
bool sTracerProcessed = false;

// This function is called from the main (UI) thread.
gboolean TracerCallback(gpointer data)
{
  mozilla::SignalTracerThread();
  return FALSE;
}

} // namespace

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

// This function is called from the background tracer thread.
bool FireAndWaitForTracerEvent()
{
  NS_ABORT_IF_FALSE(sMutex && sCondVar, "Tracing not initialized!");

  // Send a default-priority idle event through the
  // event loop, and wait for it to finish.
  MutexAutoLock lock(*sMutex);
  NS_ABORT_IF_FALSE(!sTracerProcessed, "Tracer synchronization state is wrong");
  g_idle_add_full(G_PRIORITY_DEFAULT,
                  TracerCallback,
                  NULL,
                  NULL);
  while (!sTracerProcessed)
    sCondVar->Wait();
  sTracerProcessed = false;
  return true;
}

void SignalTracerThread()
{
  if (!sMutex || !sCondVar)
    return;
  MutexAutoLock lock(*sMutex);
  NS_ABORT_IF_FALSE(!sTracerProcessed, "Tracer synchronization state is wrong");
  sTracerProcessed = true;
  sCondVar->Notify();
}

}  // namespace mozilla
