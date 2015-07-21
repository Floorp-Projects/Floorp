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
  RP_Default                     = nsIHttpChannel::REFERRER_POLICY_DEFAULT,

  /* spec tokens: origin-when-cross-origin */
  RP_Origin_When_Crossorigin     = nsIHttpChannel::REFERRER_POLICY_ORIGIN_WHEN_XORIGIN,

  /* spec tokens: always unsafe-url */
  RP_Unsafe_URL                  = nsIHttpChannel::REFERRER_POLICY_UNSAFE_URL,

  /* referrer policy is not set */
  RP_Unset                       = nsIHttpChannel::REFERRER_POLICY_NO_REFERRER_WHEN_DOWNGRADE
};

/* spec tokens: never no-referrer */
const char kRPS_Never[]                       = "never";
const char kRPS_No_Referrer[]                 = "no-referrer";

/* spec tokens: origin */
const char kRPS_Origin[]                      = "origin";

/* spec tokens: default no-referrer-when-downgrade */
const char kRPS_Default[]                     = "default";
const char kRPS_No_Referrer_When_Downgrade[]  = "no-referrer-when-downgrade";

/* spec tokens: origin-when-cross-origin */
const char kRPS_Origin_When_Cross_Origin[]    = "origin-when-cross-origin";
const char kRPS_Origin_When_Crossorigin[]     = "origin-when-crossorigin";

/* spec tokens: always unsafe-url */
const char kRPS_Always[]                      = "always";
const char kRPS_Unsafe_URL[]                  = "unsafe-url";

inline ReferrerPolicy
ReferrerPolicyFromString(const nsAString& content)
{
  // This is implemented step by step as described in the Referrer Policy
  // specification, section 6.4 "Determine token's Policy".
  if (content.LowerCaseEqualsLiteral(kRPS_Never) ||
      content.LowerCaseEqualsLiteral(kRPS_No_Referrer)) {
    return RP_No_Referrer;
  }
  if (content.LowerCaseEqualsLiteral(kRPS_Origin)) {
    return RP_Origin;
  }
  if (content.LowerCaseEqualsLiteral(kRPS_Default) ||
      content.LowerCaseEqualsLiteral(kRPS_No_Referrer_When_Downgrade)) {
    return RP_No_Referrer_When_Downgrade;
  }
  if (content.LowerCaseEqualsLiteral(kRPS_Origin_When_Cross_Origin) ||
      content.LowerCaseEqualsLiteral(kRPS_Origin_When_Crossorigin)) {
    return RP_Origin_When_Crossorigin;
  }
  if (content.LowerCaseEqualsLiteral(kRPS_Always) ||
      content.LowerCaseEqualsLiteral(kRPS_Unsafe_URL)) {
    return RP_Unsafe_URL;
  }
  // Spec says if none of the previous match, use No_Referrer.
  return RP_No_Referrer;

}

inline bool
IsValidReferrerPolicy(const nsAString& content)
{
  return content.LowerCaseEqualsLiteral(kRPS_Never)
      || content.LowerCaseEqualsLiteral(kRPS_No_Referrer)
      || content.LowerCaseEqualsLiteral(kRPS_Origin)
      || content.LowerCaseEqualsLiteral(kRPS_Default)
      || content.LowerCaseEqualsLiteral(kRPS_No_Referrer_When_Downgrade)
      || content.LowerCaseEqualsLiteral(kRPS_Origin_When_Cross_Origin)
      || content.LowerCaseEqualsLiteral(kRPS_Origin_When_Crossorigin)
      || content.LowerCaseEqualsLiteral(kRPS_Always)
      || content.LowerCaseEqualsLiteral(kRPS_Unsafe_URL);
}

inline bool
IsValidAttributeReferrerPolicy(const nsAString& aContent)
{
  // Spec allows only these three policies at the moment
  // See bug 1178337
  return aContent.LowerCaseEqualsLiteral(kRPS_No_Referrer)
      || aContent.LowerCaseEqualsLiteral(kRPS_Origin)
      || aContent.LowerCaseEqualsLiteral(kRPS_Unsafe_URL);
}

inline ReferrerPolicy
AttributeReferrerPolicyFromString(const nsAString& aContent)
{
  // if the referrer attribute string is empty, return RP_Unset
  if (aContent.IsEmpty()) {
    return RP_Unset;
  }
  // if the referrer attribute string is not empty and contains a valid
  // referrer policy, return the according enum value
  if (IsValidAttributeReferrerPolicy(aContent)) {
    return ReferrerPolicyFromString(aContent);
  }
  // in any other case the referrer attribute contains an invalid
  // policy value, we thus return RP_No_Referrer
  return RP_No_Referrer;
}

} // namespace net
} // namespace mozilla

#endif
