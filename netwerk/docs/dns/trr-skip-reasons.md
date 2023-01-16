# TRRSkippedReasons

These values are defined in [TRRSkippedReason.h](https://searchfox.org/mozilla-central/source/netwerk/dns/nsITRRSkipReason.idl) and are recorded on _nsHostRecord_ for each resolution.
We normally use them for telemetry or to determine the cause of a TRR failure.


## TRR_UNSET

Value: 0

This reason is set on _nsHostRecord_ before we attempt to resolve the domain.
Normally we should not report this value into telemetry - if we do that means there's a bug in the code.


## TRR_OK

Value: 1

This reason is set when we got a positive TRR result. That means we used TRR for the DNS resolution, the HTTPS request got a 200 response, the response was properly decoded as a DNS packet and that packet contained relevant answers.


## TRR_NO_GSERVICE

Value: 2

This reason is only set if there is no TRR service instance when trying to compute the TRR mode for a request. It indicates a bug in the implementation.


## TRR_PARENTAL_CONTROL

Value: 3

This reason is set when we have detected system level parental controls are enabled. In this case we will not be using TRR for any requests.


## TRR_OFF_EXPLICIT

Value: 4

This reason is set when DNS over HTTPS has been explicitly disabled by the user (by setting _network.trr.mode_ to _5_). In this case we will not be using TRR for any requests.


## TRR_REQ_MODE_DISABLED

Value: 5

The request had the _nsIRequest::TRRMode_ set to _TRR\_DISABLED\_MODE_. That is usually the case for request that should not use TRR, such as the TRRServiceChannel, captive portal and connectivity checks, DoHHeuristics checks, requests originating from PAC scripts, etc.


## TRR_MODE_NOT_ENABLED

Value: 6

This reason is set when the TRRService is not enabled. The only way we would end up reporting this to telemetry would be if the TRRService was enabled when the request was dispatched, but by the time it was processed the TRRService was disabled.


## TRR_FAILED

Value: 7

The TRR request failed for an unknown reason.


## TRR_MODE_UNHANDLED_DEFAULT

Value: 8

This reason is no longer used. This value may be recycled to mean something else in the future.


## TRR_MODE_UNHANDLED_DISABLED

Value: 9

This reason is no longer used. This value may be recycled to mean something else in the future.


## TRR_DISABLED_FLAG

Value: 10

This reason is used when retrying failed connections, sync DNS resolves on the main thread, or requests coming from webextensions that choose to skip TRR.


## TRR_TIMEOUT

Value: 11

The TRR request timed out.

## TRR_CHANNEL_DNS_FAIL

Value: 12

This reason is set when we fail to resolve the name of the DNS over HTTPS server.


## TRR_IS_OFFLINE

Value: 13

This reason is recorded when the TRR request fails and the browser is offline (no active interfaces).


## TRR_NOT_CONFIRMED

Value: 14

This reason is recorded when the TRR Service is not yet confirmed to work. Confirmation is only enabled when _Do53_ fallback is enabled.


## TRR_DID_NOT_MAKE_QUERY

Value: 15

This reason is sent when _TrrLookup_ exited without doing a TRR query. It may be set during shutdown, or may indicate an implementation bug.


## TRR_UNKNOWN_CHANNEL_FAILURE

Value: 16

The TRR request failed with an unknown channel failure reason.


## TRR_HOST_BLOCKED_TEMPORARY

Value: 17

The reason is recorded when the host is temporarily blocked. This happens when a previous attempt to resolve it with TRR failed, but fallback to _Do53_ succeeded.


## TRR_SEND_FAILED

Value: 18

The call to TRR::SendHTTPRequest failed.


## TRR_NET_RESET

Value: 19

The request failed because the connection to the TRR server was reset.


## TRR_NET_TIMEOUT

Value: 20

The request failed because the connection to the TRR server timed out.


## TRR_NET_REFUSED

Value: 21

The request failed because the connection to the TRR server was refused.


## TRR_NET_INTERRUPT

Value: 22

The request failed because the connection to the TRR server was interrupted.


## TRR_NET_INADEQ_SEQURITY

Value: 23

The request failed because the connection to the TRR server used an invalid TLS configuration.


## TRR_NO_ANSWERS

Value: 24

The TRR request succeeded but the encoded DNS packet contained no answers.


## TRR_DECODE_FAILED

Value: 25

The TRR request succeeded but decoding the DNS packet failed.


## TRR_EXCLUDED

Value: 26

This reason is set when the domain being resolved is excluded from TRR, either via the _network.trr.excluded-domains_ pref or because it was covered by the DNS Suffix of the user's network.


## TRR_SERVER_RESPONSE_ERR

Value: 27

The server responded with non-200 code.


## TRR_RCODE_FAIL

Value: 28

The decoded DNS packet contains an rcode that is different from NOERROR.


## TRR_NO_CONNECTIVITY

Value: 29

This reason is set when the browser has no connectivity.


## TRR_NXDOMAIN

Value: 30

This reason is set when the DNS response contains NXDOMAIN rcode (0x03).


## TRR_REQ_CANCELLED

Value: 31

This reason is set when the request was cancelled prior to completion.

## ODOH_KEY_NOT_USABLE

Value: 32

This reason is set when we don't have a valid ODoHConfig to use.

## ODOH_UPDATE_KEY_FAILED

Value: 33

This reason is set when we failed to update the ODoHConfigs.

## ODOH_KEY_NOT_AVAILABLE

Value: 34

This reason is set when ODoH requests timeout because of no key.

## ODOH_ENCRYPTION_FAILED

Value: 35

This reason is set when we failed to encrypt DNS packets.

## ODOH_DECRYPTION_FAILED

Value: 36

This reason is set when we failed to decrypt DNS packets.

## TRR_HEURISTIC_TRIPPED_GOOGLE_SAFESEARCH

Value: 37

This reason is set when the google safesearch heuristic was tripped.

## TRR_HEURISTIC_TRIPPED_YOUTUBE_SAFESEARCH

Value: 38

This reason is set when the youtube safesearch heuristic was tripped.

## TRR_HEURISTIC_TRIPPED_ZSCALER_CANARY

Value: 39

This reason is set when the zscaler canary heuristic was tripped.

## TRR_HEURISTIC_TRIPPED_CANARY

Value: 40

This reason is set when the global canary heuristic was tripped.

## TRR_HEURISTIC_TRIPPED_MODIFIED_ROOTS

Value: 41

This reason is set when the modified roots (enterprise_roots cert pref) heuristic was tripped.

## TRR_HEURISTIC_TRIPPED_PARENTAL_CONTROLS

Value: 42

This reason is set when parental controls are detected.

## TRR_HEURISTIC_TRIPPED_THIRD_PARTY_ROOTS

Value: 43

This reason is set when the third party roots heuristic was tripped.

## TRR_HEURISTIC_TRIPPED_ENTERPRISE_POLICY

Value: 44

This reason is set when enterprise policy heuristic was tripped.

## TRR_HEURISTIC_TRIPPED_VPN

Value: 45

This reason is set when the heuristic was tripped by a vpn being detected.

## TRR_HEURISTIC_TRIPPED_PROXY

Value: 46

This reason is set when the heuristic was tripped by a proxy being detected.

## TRR_HEURISTIC_TRIPPED_NRPT

Value: 47

This reason is set when the heuristic was tripped by a NRPT being detected.
