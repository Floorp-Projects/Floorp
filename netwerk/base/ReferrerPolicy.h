/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ReferrerPolicy_h__
#define ReferrerPolicy_h__

#include "nsString.h"
#include "nsIHttpChannel.h"
#include "nsUnicharUtils.h"

namespace mozilla { namespace net {

enum ReferrerPolicy {
  /* spec tokens: never no-referrer */
  RP_No_Referrer                 = nsIHttpChannel::REFERRER_POLICY_NO_REFERRER,

  /* spec tokens: origin */
  RP_Origin                      = nsIHttpChannel::REFERRER_POLICY_ORIGIN,

  /* spec tokens: default no-referrer-when-downgrade */
  RP_No_Referrer_When_Downgrade  = nsIHttpChannel::REFERRER_POLICY_NO_REFERRER_WHEN_DOWNGRADE,

  /* spec tokens: origin-when-cross-origin */
  RP_Origin_When_Crossorigin     = nsIHttpChannel::REFERRER_POLICY_ORIGIN_WHEN_XORIGIN,

  /* spec tokens: always unsafe-url */
  RP_Unsafe_URL                  = nsIHttpChannel::REFERRER_POLICY_UNSAFE_URL,

  /* spec tokens: same-origin */
  RP_Same_Origin                = nsIHttpChannel::REFERRER_POLICY_SAME_ORIGIN,

  /* spec tokens: strict-origin */
  RP_Strict_Origin               = nsIHttpChannel::REFERRER_POLICY_STRICT_ORIGIN,

  /* spec tokens: strict-origin-when-cross-origin */
  RP_Strict_Origin_When_Cross_Origin = nsIHttpChannel::REFERRER_POLICY_STRICT_ORIGIN_WHEN_XORIGIN,

  /* spec tokens: empty string */
  /* The empty string "" corresponds to no referrer policy, or unset policy */
  RP_Unset                       = nsIHttpChannel::REFERRER_POLICY_UNSET,
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

/* spec tokens: same-origin */
const char kRPS_Same_Origin[]                 = "same-origin";

/* spec tokens: strict-origin */
const char kRPS_Strict_Origin[]               = "strict-origin";

/* spec tokens: strict-origin-when-cross-origin */
const char kRPS_Strict_Origin_When_Cross_Origin[] = "strict-origin-when-cross-origin";

/* spec tokens: always unsafe-url */
const char kRPS_Always[]                      = "always";
const char kRPS_Unsafe_URL[]                  = "unsafe-url";

inline ReferrerPolicy
ReferrerPolicyFromString(const nsAString& content)
{
  if (content.IsEmpty()) {
    return RP_No_Referrer;
  }

  nsString lowerContent(content);
  ToLowerCase(lowerContent);
  // This is implemented step by step as described in the Referrer Policy
  // specification, section "Determine token's Policy".
  if (lowerContent.EqualsLiteral(kRPS_Never) ||
      lowerContent.EqualsLiteral(kRPS_No_Referrer)) {
    return RP_No_Referrer;
  }
  if (lowerContent.EqualsLiteral(kRPS_Origin)) {
    return RP_Origin;
  }
  if (lowerContent.EqualsLiteral(kRPS_Default) ||
      lowerContent.EqualsLiteral(kRPS_No_Referrer_When_Downgrade)) {
    return RP_No_Referrer_When_Downgrade;
  }
  if (lowerContent.EqualsLiteral(kRPS_Origin_When_Cross_Origin) ||
      lowerContent.EqualsLiteral(kRPS_Origin_When_Crossorigin)) {
    return RP_Origin_When_Crossorigin;
  }
  if (lowerContent.EqualsLiteral(kRPS_Same_Origin)) {
    return RP_Same_Origin;
  }
  if (lowerContent.EqualsLiteral(kRPS_Strict_Origin)) {
    return RP_Strict_Origin;
  }
  if (lowerContent.EqualsLiteral(kRPS_Strict_Origin_When_Cross_Origin)) {
    return RP_Strict_Origin_When_Cross_Origin;
  }
  if (lowerContent.EqualsLiteral(kRPS_Always) ||
      lowerContent.EqualsLiteral(kRPS_Unsafe_URL)) {
    return RP_Unsafe_URL;
  }
  // Spec says if none of the previous match, use empty string.
  return RP_Unset;

}

inline bool
IsValidReferrerPolicy(const nsAString& content)
{
  if (content.IsEmpty()) {
    return true;
  }

  nsString lowerContent(content);
  ToLowerCase(lowerContent);

  return lowerContent.EqualsLiteral(kRPS_Never)
      || lowerContent.EqualsLiteral(kRPS_No_Referrer)
      || lowerContent.EqualsLiteral(kRPS_Origin)
      || lowerContent.EqualsLiteral(kRPS_Default)
      || lowerContent.EqualsLiteral(kRPS_No_Referrer_When_Downgrade)
      || lowerContent.EqualsLiteral(kRPS_Origin_When_Cross_Origin)
      || lowerContent.EqualsLiteral(kRPS_Origin_When_Crossorigin)
      || lowerContent.EqualsLiteral(kRPS_Same_Origin)
      || lowerContent.EqualsLiteral(kRPS_Strict_Origin)
      || lowerContent.EqualsLiteral(kRPS_Strict_Origin_When_Cross_Origin)
      || lowerContent.EqualsLiteral(kRPS_Always)
      || lowerContent.EqualsLiteral(kRPS_Unsafe_URL);
}

inline ReferrerPolicy
AttributeReferrerPolicyFromString(const nsAString& content)
{
  // Specs : https://html.spec.whatwg.org/multipage/infrastructure.html#referrer-policy-attribute
  // Spec says the empty string "" corresponds to no referrer policy, or RP_Unset
  if (content.IsEmpty()) {
    return RP_Unset;
  }

  nsString lowerContent(content);
  ToLowerCase(lowerContent);

  if (lowerContent.EqualsLiteral(kRPS_No_Referrer)) {
    return RP_No_Referrer;
  }
  if (lowerContent.EqualsLiteral(kRPS_Origin)) {
    return RP_Origin;
  }
  if (lowerContent.EqualsLiteral(kRPS_No_Referrer_When_Downgrade)) {
    return RP_No_Referrer_When_Downgrade;
  }
  if (lowerContent.EqualsLiteral(kRPS_Origin_When_Cross_Origin)) {
    return RP_Origin_When_Crossorigin;
  }
  if (lowerContent.EqualsLiteral(kRPS_Unsafe_URL)) {
    return RP_Unsafe_URL;
  }
  if (lowerContent.EqualsLiteral(kRPS_Strict_Origin)) {
    return RP_Strict_Origin;
  }
  if (lowerContent.EqualsLiteral(kRPS_Same_Origin)) {
    return RP_Same_Origin;
  }
  if (lowerContent.EqualsLiteral(kRPS_Strict_Origin_When_Cross_Origin)) {
    return RP_Strict_Origin_When_Cross_Origin;
  }

  // Spec says invalid value default is empty string state
  // So, return RP_Unset if none of the previous match, return RP_Unset
  return RP_Unset;
}

} // namespace net
} // namespace mozilla

#endif
