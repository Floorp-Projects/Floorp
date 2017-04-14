/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_ScreenManager_h
#define mozilla_widget_ScreenManager_h

#include "nsIScreenManager.h"

#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/widget/Screen.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {
class ContentParent;
class ScreenDetails;
}
}

namespace mozilla {
namespace widget {

class ScreenManager final : public nsIScreenManager
{
public:
  class Helper
  {
  public:
    virtual ~Helper() {}
  };

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISCREENMANAGER

  static ScreenManager& GetSingleton();
  static already_AddRefed<ScreenManager> GetAddRefedSingleton();

  void SetHelper(UniquePtr<Helper> aHelper);
  void Refresh(nsTArray<RefPtr<Screen>>&& aScreens);
  void Refresh(nsTArray<mozilla::dom::ScreenDetails>&& aScreens);
  void CopyScreensToRemote(mozilla::dom::ContentParent* aContentParent);

private:
  ScreenManager();
  virtual ~ScreenManager();

  template<class Range>
  void CopyScreensToRemoteRange(Range aRemoteRange);
  void CopyScreensToAllRemotesIfIsParent();

  AutoTArray<RefPtr<Screen>, 4> mScreenList;
  UniquePtr<Helper> mHelper;
};


} // namespace widget
} // namespace mozilla

#endif // mozilla_widget_ScreenManager_h
