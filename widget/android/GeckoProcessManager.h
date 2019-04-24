/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GeckoProcessManager_h
#define GeckoProcessManager_h

#include "GeneratedJNINatives.h"
#include "WidgetUtils.h"
#include "nsAppShell.h"
#include "nsWindow.h"

#include "mozilla/RefPtr.h"
#include "mozilla/dom/ContentProcessManager.h"

namespace mozilla {

class GeckoProcessManager final
    : public java::GeckoProcessManager::Natives<GeckoProcessManager> {
  GeckoProcessManager() = delete;

  static already_AddRefed<nsIWidget> GetWidget(int64_t aContentId,
                                               int64_t aTabId) {
    using namespace dom;
    MOZ_ASSERT(NS_IsMainThread());

    ContentProcessManager* const cpm = ContentProcessManager::GetSingleton();
    NS_ENSURE_TRUE(cpm, nullptr);

    RefPtr<BrowserParent> tab = cpm->GetTopLevelBrowserParentByProcessAndTabId(
        ContentParentId(aContentId), TabId(aTabId));
    NS_ENSURE_TRUE(tab, nullptr);

    nsCOMPtr<nsPIDOMWindowOuter> domWin = tab->GetParentWindowOuter();
    NS_ENSURE_TRUE(domWin, nullptr);

    return widget::WidgetUtils::DOMWindowToWidget(domWin);
  }

 public:
  static void GetEditableParent(jni::Object::Param aEditableChild,
                                int64_t aContentId, int64_t aTabId) {
    nsCOMPtr<nsIWidget> widget = GetWidget(aContentId, aTabId);
    if (RefPtr<nsWindow> window = nsWindow::From(widget)) {
      java::GeckoProcessManager::SetEditableChildParent(
          aEditableChild, window->GetEditableParent());
    }
  }
};

}  // namespace mozilla

#endif  // GeckoProcessManager_h
