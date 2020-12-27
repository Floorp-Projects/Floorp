/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_widget_WinContentSystemParameters_h
#define mozilla_widget_WinContentSystemParameters_h
#include <memory>
#include <cinttypes>
#include "nsTArray.h"

namespace mozilla {
namespace dom {

class SystemParameterKVPair;

}

namespace widget {

enum class SystemParameterId : uint8_t {
  IsPerMonitorDPIAware = 0,
  SystemDPI,
  FlatMenusEnabled,
  IsAppThemed,
  Count,
};

class WinContentSystemParameters {
 public:
  static WinContentSystemParameters* GetSingleton();

  bool IsPerMonitorDPIAware();

  float SystemDPI();

  bool AreFlatMenusEnabled();

  bool IsAppThemed();

  void SetContentValues(const nsTArray<dom::SystemParameterKVPair>& values);

  nsTArray<dom::SystemParameterKVPair> GetParentValues();

  void OnThemeChanged();

  WinContentSystemParameters(const WinContentSystemParameters&) = delete;
  WinContentSystemParameters(WinContentSystemParameters&&) = delete;
  WinContentSystemParameters& operator=(const WinContentSystemParameters&) =
      delete;
  WinContentSystemParameters& operator=(WinContentSystemParameters&&) = delete;

 private:
  WinContentSystemParameters();
  ~WinContentSystemParameters();

  void SetContentValueInternal(const dom::SystemParameterKVPair& aKVPair);

  bool GetParentValueInternal(SystemParameterId aId,
                              dom::SystemParameterKVPair* aKVPair);

  bool IsCachedValueValid(SystemParameterId aId);
  void SetCachedValueValid(SystemParameterId aId, bool aIsValid);

  struct Detail;
  std::unique_ptr<Detail> mDetail;
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_WinContentSystemParameters_h
