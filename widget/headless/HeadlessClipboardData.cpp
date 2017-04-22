/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HeadlessClipboardData.h"

namespace mozilla {
namespace widget {

void
HeadlessClipboardData::SetText(const nsAString &aText)
{
  mPlain = aText;
}

bool
HeadlessClipboardData::HasText() const
{
  return !mPlain.IsEmpty();
}

const nsAString&
HeadlessClipboardData::GetText() const
{
  return mPlain;
}

void
HeadlessClipboardData::Clear()
{
  mPlain.Truncate(0);
}

} // namespace widget
} // namespace mozilla
