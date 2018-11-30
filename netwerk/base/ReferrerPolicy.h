/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ReferrerPolicy_h__
#define ReferrerPolicy_h__

#include "nsString.h"
#include "nsIHttpChannel.h"
#include "nsUnicharUtils.h"

namespace mozilla {
namespace net {

enum ReferrerPolicy {
  /* spec tokens: never no-referrer */
  RP_No_Referrer = nsIHttpChannel::REFERRER_POLICY_NO_REFERRER,

  /* spec tokens: origin */
  RP_Origin = nsIHttpChannel::REFERRER_POLICY_ORIGIN,

  /* spec tokens: default no-referrer-when-downgrade */
  RP_No_Referrer_When_Downgrade =
      nsIHttpChannel::REFERRER_POLICY_NO_REFERRER_WHEN_DOWNGRADE,

  /* spec tokens: origin-when-cross-origin */
  RP_Origin_When_Crossorigin =
      nsIHttpChannel::REFERRER_POLICY_ORIGIN_WHEN_XORIGIN,

  /* spec tokens: always unsafe-url */
  RP_Unsafe_URL = nsIHttpChannel::REFERRER_POLICY_UNSAFE_URL,

  /* spec tokens: same-origin */
  RP_Same_Origin = nsIHttpChannel::REFERRER_POLICY_SAME_ORIGIN,

  /* spec tokens: strict-origin */
  RP_Strict_Origin = nsIHttpChannel::REFERRER_POLICY_STRICT_ORIGIN,

  /* spec tokens: strict-origin-when-cross-origin */
  RP_Strict_Origin_When_Cross_Origin =
      nsIHttpChannel::REFERRER_POLICY_STRICT_ORIGIN_WHEN_XORIGIN,

  /* spec tokens: empty string */
  /* The empty string "" corresponds to no referrer policy, or unset policy */
  RP_Unset = nsIHttpChannel::REFERRER_POLICY_UNSET,
};

// Referrer Policy spec tokens. Order matters here, make sure it matches the
// order as in nsIHttpChannel.idl
static const char* kReferrerPolicyString[] = {
    "",                            // REFERRER_POLICY_UNSET
    "no-referrer-when-downgrade",  // REFERRER_POLICY_NO_REFERRER_WHEN_DOWNGRADE
    "no-referrer",                 // REFERRER_POLICY_NO_REFERRER
    "origin",                      // REFERRER_POLICY_ORIGIN
    "origin-when-cross-origin",    // REFERRER_POLICY_ORIGIN_WHEN_XORIGIN
    "unsafe-url",                  // REFERRER_POLICY_UNSAFE_URL
    "same-origin",                 // REFERRER_POLICY_SAME_ORIGIN
    "strict-origin",               // REFERRER_POLICY_STRICT_ORIGIN
    "strict-origin-when-cross-origin"  // REFERRER_POLICY_STRICT_ORIGIN_WHEN_XORIGIN
};

/* spec tokens: never */
const char kRPS_Never[] = "never";

/* spec tokens: default */
const char kRPS_Default[] = "default";

/* spec tokens: origin-when-crossorigin */
const char kRPS_Origin_When_Crossorigin[] = "origin-when-crossorigin";

/* spec tokens: always */
const char kRPS_Always[] = "always";

inline ReferrerPolicy ReferrerPolicyFromString(const nsAString& content) {
  if (content.IsEmpty()) {
    return RP_No_Referrer;
  }

  nsString lowerContent(content);
  ToLowerCase(lowerContent);
  // This is implemented step by step as described in the Referrer Policy
  // specification, section "Determine token's Policy".

  uint16_t numStr =
      (sizeof(kReferrerPolicyString) / sizeof(kReferrerPolicyString[0]));
  for (uint16_t i = 0; i < numStr; i++) {
    if (lowerContent.EqualsASCII(kReferrerPolicyString[i])) {
      return static_cast<ReferrerPolicy>(i);
    }
  }

  if (lowerContent.EqualsLiteral(kRPS_Never)) {
    return RP_No_Referrer;
  }
  if (lowerContent.EqualsLiteral(kRPS_Default)) {
    return RP_No_Referrer_When_Downgrade;
  }
  if (lowerContent.EqualsLiteral(kRPS_Origin_When_Crossorigin)) {
    return RP_Origin_When_Crossorigin;
  }
  if (lowerContent.EqualsLiteral(kRPS_Always)) {
    return RP_Unsafe_URL;
  }
  // Spec says if none of the previous match, use empty string.
  return RP_Unset;
}

inline ReferrerPolicy AttributeReferrerPolicyFromString(
    const nsAString& content) {
  // Specs :
  // https://html.spec.whatwg.org/multipage/infrastructure.html#referrer-policy-attribute
  // Spec says the empty string "" corresponds to no referrer policy, or
  // RP_Unset
  if (content.IsEmpty()) {
    return RP_Unset;
  }

  nsString lowerContent(content);
  ToLowerCase(lowerContent);

  uint16_t numStr =
      (sizeof(kReferrerPolicyString) / sizeof(kReferrerPolicyString[0]));
  for (uint16_t i = 0; i < numStr; i++) {
    if (lowerContent.EqualsASCII(kReferrerPolicyString[i])) {
      return static_cast<ReferrerPolicy>(i);
    }
  }

  // Spec says invalid value default is empty string state
  // So, return RP_Unset if none of the previous match, return RP_Unset
  return RP_Unset;
}

inline const char* ReferrerPolicyToString(ReferrerPolicy aPolicy) {
  return kReferrerPolicyString[static_cast<uint32_t>(aPolicy)];
}

}  // namespace net
}  // namespace mozilla

#endif
