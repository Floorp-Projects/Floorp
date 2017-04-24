/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef mozilla_widget_HeadlessClipboard_h
#define mozilla_widget_HeadlessClipboard_h

#include "nsIClipboard.h"
#include "mozilla/UniquePtr.h"
#include "HeadlessClipboardData.h"

namespace mozilla {
namespace widget {

class HeadlessClipboard final : public nsIClipboard
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICLIPBOARD

  HeadlessClipboard();

protected:
  ~HeadlessClipboard() {}

private:
  UniquePtr<HeadlessClipboardData> mClipboard;
};

} // namespace widget
} // namespace mozilla

#endif
