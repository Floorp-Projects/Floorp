DNS over HTTPS (Trusted Recursive Resolver)
===========================================

Terminology
-----------

**DNS-over-HTTPS (DoH)** allows DNS to be resolved with enhanced privacy, secure transfers and comparable performance. The protocol is described in `RFC 8484 <https://tools.ietf.org/html/rfc8484>`_ .


**Trusted Recursive Resolver (TRR)** is the name of Firefox's implementation
of the protocol and the `policy <https://wiki.mozilla.org/Security/DOH-resolver-policy>`_ that ensures only privacy-respecting DoH providers are recommended by Firefox.

On this page we will use DoH when referring to the protocol, and TRR when referring to the implementation.

**Unencrypted DNS (Do53)** is the regular way most programs resolve DNS names. This is usually done by the operating system by sending an unencrypted packet to the DNS server
that normally listens on port 53.

DoH Rollout
-----------

**DoH Rollout** refers to the webextension code that decides whether TRR will
be enabled automatically for users in the `rollout population <https://support.mozilla.org/kb/firefox-dns-over-https#w_about-the-us-rollout-of-dns-over-https>`_.

The functioning of this module is described `here <https://wiki.mozilla.org/Security/DNS_Over_HTTPS>`_.

Implementation
--------------

When enabled TRR may work in two modes, TRR-first (2) and TRR-only (3). These are controlled by the **network.trr.mode** or **doh-rollout.mode** prefs.
The difference is that when a DoH request fails in TRR-first mode, we then fallback to **Do53**.

DNS name resolutions are performed in nsHostResolver::ResolveHost. If a cached response for the request could not be found, nsHostResolver::NameLookup will trigger either
a DoH or a Do53 request. First it checks the effective TRR mode of the request
is as requests could have a different mode from the global one.
If the request may use TRR, then we dispatch a request in nsHostResolver::TrrLookup.
Since we usually reolve both IPv4 and IPv6 names, a **TRRQuery** object is
created to perform and combine both responses. 

Once done, nsHostResolver::CompleteLookup is called. If the DoH server returned a
valid response we use it, otherwise we report a failure in TRR-only mode, or
try Do53 in TRR-first mode.

**TRRService** controls the global state and settings of the feature.
Each individual request is performed by the **TRR** class.

Since HTTP channels in Firefox normally work on the main thread, TRR uses a
special implementation called **TRRServiceChannel** to avoid congestion on the
main thread.

Dynamic Blocklist
-----------------

In order to improve performance TRR service manages a dynamic persistent blocklist for host names that can't be resolved with DoH but works with the native resolver. Blocklisted entries will not be retried over DoH for one minute.

TRR confirmation
----------------

TRR requests normally have a 1.5 second timeout. If for some reason we do not
get a response in that time we fall back to Do53. To avoid this delay for all
requests when the DoH server is not accessible, we perform a confirmation check.
If the check fails, we conclude that the server is not usable and will use Do53
directly. The confirmation check is retried periodically to check if the TRR
connection is functional again.

The confirmation state has one of the following values:
  - CONFIRM_OFF: TRR is turned off, so the service is not active.
  - CONFIRM_TRING_OK: TRR in on, but we are not sure yet if the DoH server is accessible. We optimistically try to resolve via DoH and fall back to Do53 after 1.5 seconds. While in this state the TRRService will be performing NS record requests to the DoH server as a connectivity check. Depending on a successful response it will either transition to the CONFIRM_OK or CONFIRM_FAILED state.
  - CONFIRM_OK: TRR is on and we have confirmed that the DoH server is behaving adequately. Will use TRR for all requests (and fall back to Do53 in case of timeout, NXDOMAIN, etc).
  - CONFIRM_FAILED: TRR is on, but the DoH server is not accessible. Either we have no network connectivity, or the server is down. We don't perform DoH requests in this state because they are sure to fail.
  - CONFIRM_TRYING_FAILED: This is equivalent to CONFIRM_FAILED, but we periodically enter this state when rechecking if the DoH server is accessible.
  - CONFIRM_DISABLED: We are in this state if the browser is in TRR-only mode, or if the confirmation was explicitly disabled via pref.

The state machine for the confirmation is defined in the `HandleConfirmationEvent` method in `TRRService.cpp`.

Excluded domains
----------------

Some domains will never be resolved via TRR. This includes:
  - domains listed in the **network.trr.builtin-excluded-domains** pref (normally domains that are equal or end in *localhost* or *local*)
  - domains listed in the **network.trr.excluded-domains** pref (chosen by the user)
  - domains that are subdomains of the network's DNS suffix (for example if the network has the **lan** suffix, domains such as **computer.lan** will not use TRR)
  - requests made by Firefox to check for the existence of a captive-portal
  - requests made by Firefox to check the network's IPv6 capabilities
  - domains listed in `/etc/hosts`

Steering
--------

A small set of TRR providers are only available on certain networks. Detection is performed in DoHHeuristics.jsm followed by a call to `TRRService::SetDetectedURI`.
This causes Firefox to use the network specific TRR provider until a network change occurs.

User choice
-----------

The TRR feature is designed to prioritize user choice before user agent decisions. That means the user may explicitly disable TRR by setting **network.trr.mode** to **5** (TRR-disabled), and that `doh-rollout` will not overwrite user settings.
Changes to the TRR URL or TRR mode by the user will disable heuristics use the user configured settings.
