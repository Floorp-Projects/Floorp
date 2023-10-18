/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HeadlessClipboardData.h"

namespace mozilla::widget {

void HeadlessClipboardData::SetText(const nsAString& aText) {
  mPlain = aText;
  mChangeCount++;
}

bool HeadlessClipboardData::HasText() const { return !mPlain.IsVoid(); }

const nsAString& HeadlessClipboardData::GetText() const { return mPlain; }

void HeadlessClipboardData::SetHTML(const nsAString& aHTML) {
  mHTML = aHTML;
  mChangeCount++;
}

bool HeadlessClipboardData::HasHTML() const { return !mHTML.IsVoid(); }

const nsAString& HeadlessClipboardData::GetHTML() const { return mHTML; }

int32_t HeadlessClipboardData::GetChangeCount() const { return mChangeCount; }

void HeadlessClipboardData::Clear() {
  mPlain.SetIsVoid(true);
  mHTML.SetIsVoid(true);
  mChangeCount++;
}

}  // namespace mozilla::widget
