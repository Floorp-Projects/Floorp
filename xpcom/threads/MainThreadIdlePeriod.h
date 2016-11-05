/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mainthreadidleperiod_h
#define mozilla_dom_mainthreadidleperiod_h

#include "mozilla/TimeStamp.h"
#include "nsThreadUtils.h"

namespace mozilla {

class MainThreadIdlePeriod final : public IdlePeriod
{
public:
  NS_DECL_NSIIDLEPERIOD

  static float GetLongIdlePeriod();
  static float GetMinIdlePeriod();
private:
  virtual ~MainThreadIdlePeriod() {}
};

} // namespace mozilla

#endif // mozilla_dom_mainthreadidleperiod_h
