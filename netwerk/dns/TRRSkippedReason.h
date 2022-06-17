/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_TRRSkippedReason_h__
#define mozilla_net_TRRSkippedReason_h__

namespace mozilla {
namespace net {

// IMPORTANT: when adding new values, always add them to the end, otherwise
// it will mess up telemetry.
enum class TRRSkippedReason : uint32_t {
  TRR_UNSET = 0,
  TRR_OK = 1,           // Only set when we actually got a positive TRR result
  TRR_NO_GSERVICE = 2,  // no gService
  TRR_PARENTAL_CONTROL = 3,         // parental control is on
  TRR_OFF_EXPLICIT = 4,             // user has set mode5
  TRR_REQ_MODE_DISABLED = 5,        // request  has disabled flags set
  TRR_MODE_NOT_ENABLED = 6,         // mode0
  TRR_FAILED = 7,                   // unknown failure
  TRR_MODE_UNHANDLED_DEFAULT = 8,   // Unhandled case in ComputeEffectiveMode
  TRR_MODE_UNHANDLED_DISABLED = 9,  // Unhandled case in ComputeEffectiveMode
  TRR_DISABLED_FLAG = 10,           // the DISABLE_TRR flag was set
  TRR_TIMEOUT = 11,                 // the TRR channel timed out
  TRR_CHANNEL_DNS_FAIL = 12,        // DoH server name failed to resolve
  TRR_IS_OFFLINE = 13,              // The browser is offline/no interfaces up
  TRR_NOT_CONFIRMED = 14,           // TRR confirmation is not done yet
  TRR_DID_NOT_MAKE_QUERY = 15,  // TrrLookup exited without doing a TRR query
  TRR_UNKNOWN_CHANNEL_FAILURE = 16,  // unknown channel failure reason
  TRR_HOST_BLOCKED_TEMPORARY = 17,   // host blocklisted
  TRR_SEND_FAILED = 18,              // The call to TRR::SendHTTPRequest failed
  TRR_NET_RESET = 19,                // NS_ERROR_NET_RESET
  TRR_NET_TIMEOUT = 20,              // NS_ERROR_NET_TIMEOUT
  TRR_NET_REFUSED = 21,              // NS_ERROR_CONNECTION_REFUSED
  TRR_NET_INTERRUPT = 22,            // NS_ERROR_NET_INTERRUPT
  TRR_NET_INADEQ_SEQURITY = 23,      // NS_ERROR_NET_INADEQUATE_SECURITY
  TRR_NO_ANSWERS = 24,               // TRR returned no answers
  TRR_DECODE_FAILED = 25,            // DohDecode failed
  TRR_EXCLUDED = 26,                 // ExcludedFromTRR
  TRR_SERVER_RESPONSE_ERR = 27,      // Server responded with non-200 code
  TRR_RCODE_FAIL = 28,          // DNS response contains a non-NOERROR rcode
  TRR_NO_CONNECTIVITY = 29,     // Not confirmed because of no connectivity
  TRR_NXDOMAIN = 30,            // DNS response contains NXDOMAIN rcode (0x03)
  TRR_REQ_CANCELLED = 31,       // The request has been cancelled
  ODOH_KEY_NOT_USABLE = 32,     // We don't have a valid ODoHConfig to use.
  ODOH_UPDATE_KEY_FAILED = 33,  // Failed to update the ODoHConfigs.
  ODOH_KEY_NOT_AVAILABLE = 34,  // ODoH requests timeout because of no key.
  ODOH_ENCRYPTION_FAILED = 35,  // Failed to encrypt DNS packets.
  ODOH_DECRYPTION_FAILED = 36,  // Failed to decrypt DNS packets.
};

inline bool IsRelevantTRRSkipReason(TRRSkippedReason aReason) {
  // - TRR_REQ_MODE_DISABLED - these requests are intentionally skipping TRR.
  //     These include DNS queries used to bootstrap the TRR connection,
  //     captive portal checks, connectivity checks, etc.
  //     Since we don't want to use TRR for these connections, we don't need
  //     to include them with other relevant skip reasons.
  // - TRR_DISABLED_FLAG - This reason is used when retrying failed connections,
  //    sync DNS resolves on the main thread, or requests coming from
  //    webextensions that choose to skip TRR
  // - TRR_EXCLUDED - This reason is used when a certain domain is excluded
  //    from TRR because it is explicitly set by the user, or because it
  //    is part of the user's DNS suffix list, indicating a host that is likely
  //    to be on the local network.
  if (aReason == TRRSkippedReason::TRR_REQ_MODE_DISABLED ||
      aReason == TRRSkippedReason::TRR_DISABLED_FLAG ||
      aReason == TRRSkippedReason::TRR_EXCLUDED) {
    return false;
  }
  return true;
}

inline bool IsBlockedTRRRequest(TRRSkippedReason aReason) {
  // See TRR::MaybeBlockRequest. These are the reasons that could block sending
  // TRR requests.
  return (aReason == TRRSkippedReason::TRR_EXCLUDED ||
          aReason == TRRSkippedReason::TRR_MODE_NOT_ENABLED ||
          aReason == TRRSkippedReason::TRR_HOST_BLOCKED_TEMPORARY);
}

inline bool IsNonRecoverableTRRSkipReason(TRRSkippedReason aReason) {
  // These are non-recoverable reasons and we'll fallback to native without
  // retrying.
  return (aReason == TRRSkippedReason::TRR_NXDOMAIN ||
          aReason == TRRSkippedReason::TRR_NO_ANSWERS ||
          aReason == TRRSkippedReason::TRR_DISABLED_FLAG ||
          aReason == TRRSkippedReason::TRR_RCODE_FAIL);
}

inline bool IsFailedConfirmationOrNoConnectivity(TRRSkippedReason aReason) {
  // TRR is in non-confirmed state now, so we don't try to use TRR at all.
  return (aReason == TRRSkippedReason::TRR_NOT_CONFIRMED ||
          aReason == TRRSkippedReason::TRR_NO_CONNECTIVITY);
}

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_TRRSkippedReason_h__
