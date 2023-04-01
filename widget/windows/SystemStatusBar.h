/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef widget_windows_SystemStatusBar_h
#define widget_windows_SystemStatusBar_h

#include "nsISystemStatusBar.h"
#include "mozilla/LinkedList.h"

namespace mozilla::widget {
class StatusBarEntry;

class SystemStatusBar final : public nsISystemStatusBar {
 public:
  explicit SystemStatusBar() = default;
  NS_DECL_ISUPPORTS
  NS_DECL_NSISYSTEMSTATUSBAR

  static SystemStatusBar& GetSingleton();
  static already_AddRefed<SystemStatusBar> GetAddRefedSingleton();

  nsresult Init();

 private:
  ~SystemStatusBar() = default;
  mozilla::LinkedList<RefPtr<StatusBarEntry>> mStatusBarEntries;
};

}  // namespace mozilla::widget

#endif  // widget_windows_SystemStatusBar_h
