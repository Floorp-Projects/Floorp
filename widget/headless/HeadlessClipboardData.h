/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_HeadlessClipboardData_h
#define mozilla_widget_HeadlessClipboardData_h

#include "mozilla/RefPtr.h"
#include "nsString.h"

namespace mozilla {
namespace widget {

class HeadlessClipboardData final {
 public:
  HeadlessClipboardData() : mPlain(VoidString()), mHTML(VoidString()) {}
  ~HeadlessClipboardData() = default;

  // For text/plain
  void SetText(const nsAString& aText);
  bool HasText() const;
  const nsAString& GetText() const;

  // For text/html
  void SetHTML(const nsAString& aHTML);
  bool HasHTML() const;
  const nsAString& GetHTML() const;

  int32_t GetChangeCount() const;

  // For other APIs
  void Clear();

 private:
  nsString mPlain;
  nsString mHTML;

  int32_t mChangeCount = 0;
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_HeadlessClipboardData_h
