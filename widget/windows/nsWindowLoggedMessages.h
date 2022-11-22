/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WindowLoggedMessages_h__
#define WindowLoggedMessages_h__

#include "minwindef.h"
#include "wtypes.h"

#include "nsIWidget.h"
#include "nsStringFwd.h"

namespace mozilla::widget {

void LogWindowMessage(HWND hwnd, UINT event, bool isPreEvent, long eventCounter,
                      WPARAM wParam, LPARAM lParam, mozilla::Maybe<bool> result,
                      LRESULT retValue);
void WindowClosed(HWND hwnd);
void GetLatestWindowMessages(RefPtr<nsIWidget> windowWidget,
                             nsTArray<nsCString>& messages);

}  // namespace mozilla::widget

#endif /* WindowLoggedMessages */
