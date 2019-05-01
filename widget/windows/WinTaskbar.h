/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __WinTaskbar_h__
#define __WinTaskbar_h__

#include <windows.h>
#include <shobjidl.h>
#undef LogSeverity  // SetupAPI.h #defines this as DWORD
#include "nsIWinTaskbar.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace widget {

class WinTaskbar final : public nsIWinTaskbar {
  ~WinTaskbar();

 public:
  WinTaskbar();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWINTASKBAR

  // Registers the global app user model id for the instance.
  // See comments in WinTaskbar.cpp for more information.
  static bool RegisterAppUserModelID();
  static bool GetAppUserModelID(nsAString& aDefaultGroupId);

 private:
  bool Initialize();

  typedef HRESULT(WINAPI* SetCurrentProcessExplicitAppUserModelIDPtr)(
      PCWSTR AppID);
  ITaskbarList4* mTaskbar;
};

}  // namespace widget
}  // namespace mozilla

#endif /* __WinTaskbar_h__ */
