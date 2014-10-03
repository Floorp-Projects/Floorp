/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNetStrings.h"
#include "nsChannelProperties.h"

nsNetStrings* gNetStrings;

nsNetStrings::nsNetStrings()
  : NS_LITERAL_STRING_INIT(kChannelPolicy, NS_CHANNEL_PROP_CHANNEL_POLICY_STR)
{}


