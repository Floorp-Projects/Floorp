/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ted Mielczarek <ted.mielczarek@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozilla/WidgetTraceEvent.h"

#include <glib.h>
#include <mozilla/CondVar.h>
#include <mozilla/Mutex.h>
#include <stdio.h>

using mozilla::CondVar;
using mozilla::Mutex;
using mozilla::MutexAutoLock;

namespace {

Mutex sMutex("Event tracer thread mutex");
CondVar sCondVar(sMutex, "Event tracer thread condvar");
bool sTracerProcessed = false;

// This function is called from the main (UI) thread.
gboolean TracerCallback(gpointer data)
{
  MutexAutoLock lock(sMutex);
  NS_ABORT_IF_FALSE(!sTracerProcessed, "Tracer synchronization state is wrong");
  sTracerProcessed = true;
  sCondVar.Notify();
  return FALSE;
}

} // namespace

namespace mozilla {

// This function is called from the background tracer thread.
bool FireAndWaitForTracerEvent()
{
  // Send a default-priority idle event through the
  // event loop, and wait for it to finish.
  MutexAutoLock lock(sMutex);
  NS_ABORT_IF_FALSE(!sTracerProcessed, "Tracer synchronization state is wrong");
  g_idle_add_full(G_PRIORITY_DEFAULT,
                  TracerCallback,
                  NULL,
                  NULL);
  while (!sTracerProcessed)
    sCondVar.Wait();
  sTracerProcessed = false;
  return true;
}

}  // namespace mozilla
