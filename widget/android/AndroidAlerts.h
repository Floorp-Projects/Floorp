/* -*- Mode: c++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_AndroidAlerts_h__
#define mozilla_widget_AndroidAlerts_h__

#include "nsIAlertsService.h"

namespace mozilla {
namespace widget {

class AndroidAlerts : public nsIAlertsService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIALERTSSERVICE

  AndroidAlerts() {}

protected:
  virtual ~AndroidAlerts() {}
};

} // namespace widget
} // namespace mozilla

#endif // nsAndroidAlerts_h__
