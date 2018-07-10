/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThreadUtils.h"
#include "mozilla/PerformanceUtils.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/WorkerDebugger.h"
#include "mozilla/dom/WorkerDebuggerManager.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {

void
CollectPerformanceInfo(nsTArray<PerformanceInfo>& aMetrics)
{
   // collecting ReportPerformanceInfo from all DocGroup instances
   LinkedList<TabGroup>* tabGroups = TabGroup::GetTabGroupList();

   for (TabGroup* tabGroup = tabGroups->getFirst(); tabGroup;
       tabGroup =
         static_cast<LinkedListElement<TabGroup>*>(tabGroup)->getNext()) {
    for (auto iter = tabGroup->Iter(); !iter.Done(); iter.Next()) {
      DocGroup* docGroup = iter.Get()->mDocGroup;
      aMetrics.AppendElement(docGroup->ReportPerformanceInfo());
    }
  }

  // collecting ReportPerformanceInfo from all WorkerDebugger instances
  RefPtr<mozilla::dom::WorkerDebuggerManager> wdm = WorkerDebuggerManager::GetOrCreate();
  if (NS_WARN_IF(!wdm)) {
    return;
  }
  for (uint32_t i = 0; i < wdm->GetDebuggersLength(); i++) {
    WorkerDebugger* debugger = wdm->GetDebuggerAt(i);
    aMetrics.AppendElement(debugger->ReportPerformanceInfo());
  }
}

} // namespace
