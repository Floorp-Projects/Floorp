/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsServerTiming_h__
#define nsServerTiming_h__

#include "nsITimedChannel.h"
#include "nsString.h"
#include "nsTArray.h"

class nsServerTiming final : public nsIServerTiming
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISERVERTIMING

  nsServerTiming() = default;

  void SetName(const nsACString &aName)
  {
    mName = aName;
  }

  void SetDuration(double aDuration)
  {
    mDuration = aDuration;
  }

  void SetDescription(const nsACString &aDescription)
  {
    mDescription = aDescription;
  }

private:
  virtual ~nsServerTiming() = default;

  nsCString mName;
  double mDuration;
  nsCString mDescription;
};

namespace mozilla {
namespace net {

class ServerTimingParser
{
public:
  explicit ServerTimingParser(const nsCString &value)
    : mValue(value)
  {}
  void Parse();
  nsTArray<nsCOMPtr<nsIServerTiming>>&& TakeServerTimingHeaders();

private:
  nsCString mValue;
  nsTArray<nsCOMPtr<nsIServerTiming>> mServerTimingHeaders;
};

} // namespace net
} // namespace mozilla

#endif
