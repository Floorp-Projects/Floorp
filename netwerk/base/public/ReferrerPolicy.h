/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ReferrerPolicy_h__
#define ReferrerPolicy_h__

#include "nsStringGlue.h"
#include "nsIHttpChannel.h"

namespace mozilla { namespace net {

enum ReferrerPolicy {
  /* spec tokens: never no-referrer */
  RP_No_Referrer                 = nsIHttpChannel::REFERRER_POLICY_NO_REFERRER,

  /* spec tokens: origin */
  RP_Origin                      = nsIHttpChannel::REFERRER_POLICY_ORIGIN,

  /* spec tokens: default no-referrer-when-downgrade */
  RP_No_Referrer_When_Downgrade  = nsIHttpChannel::REFERRER_POLICY_NO_REFERRER_WHEN_DOWNGRADE,
  RP_Default                     = nsIHttpChannel::REFERRER_POLICY_NO_REFERRER_WHEN_DOWNGRADE,

  /* spec tokens: origin-when-crossorigin */
  RP_Origin_When_Crossorigin     = nsIHttpChannel::REFERRER_POLICY_ORIGIN_WHEN_XORIGIN,

  /* spec tokens: always unsafe-url */
  RP_Unsafe_URL                  = nsIHttpChannel::REFERRER_POLICY_UNSAFE_URL
};

inline ReferrerPolicy
ReferrerPolicyFromString(const nsAString& content)
{
  // This is implemented step by step as described in the Referrer Policy
  // specification, section 6.4 "Determine token's Policy".
  if (content.LowerCaseEqualsLiteral("never") ||
      content.LowerCaseEqualsLiteral("no-referrer")) {
    return RP_No_Referrer;
  }
  if (content.LowerCaseEqualsLiteral("origin")) {
    return RP_Origin;
  }
  if (content.LowerCaseEqualsLiteral("default") ||
      content.LowerCaseEqualsLiteral("no-referrer-when-downgrade")) {
    return RP_No_Referrer_When_Downgrade;
  }
  if (content.LowerCaseEqualsLiteral("origin-when-crossorigin")) {
    return RP_Origin_When_Crossorigin;
  }
  if (content.LowerCaseEqualsLiteral("always") ||
      content.LowerCaseEqualsLiteral("unsafe-url")) {
    return RP_Unsafe_URL;
  }
  // Spec says if none of the previous match, use No_Referrer.
  return RP_No_Referrer;

}

} } //namespace mozilla::net

#endif
