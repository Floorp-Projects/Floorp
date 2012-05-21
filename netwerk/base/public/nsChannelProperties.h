/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsChannelProperties_h__
#define nsChannelProperties_h__

#include "nsStringGlue.h"
#ifdef IMPL_NS_NET
#include "nsNetStrings.h"
#endif

/**
 * @file
 * This file contains constants for properties channels can expose.
 * They can be accessed by using QueryInterface to access the nsIPropertyBag
 * or nsIPropertyBag2 interface on a channel and reading the value.
 */


/**
 * Content-Length of a channel. Used instead of the nsIChannel.contentLength
 * property.
 * Not available before onStartRequest has been called.
 * Type: PRUint64
 */
#define NS_CHANNEL_PROP_CONTENT_LENGTH_STR "content-length"


/**
 * Exists to allow content policy mechanism to function properly during channel
 * redirects.  Contains security contextual information about the load.
 * Type: nsIChannelPolicy
 */
#define NS_CHANNEL_PROP_CHANNEL_POLICY_STR "channel-policy"

#ifdef IMPL_NS_NET
#define NS_CHANNEL_PROP_CONTENT_LENGTH gNetStrings->kContentLength
#define NS_CHANNEL_PROP_CHANNEL_POLICY gNetStrings->kChannelPolicy
#else
#define NS_CHANNEL_PROP_CONTENT_LENGTH \
  NS_LITERAL_STRING(NS_CHANNEL_PROP_CONTENT_LENGTH_STR)
#define NS_CHANNEL_PROP_CHANNEL_POLICY \
  NS_LITERAL_STRING(NS_CHANNEL_PROP_CHANNEL_POLICY_STR)
#endif

#endif
