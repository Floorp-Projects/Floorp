# Captive portal detection

## What are captive portals?
A captive portal is what we call a network that requires your action before it allows you to connect to the Internet. This action could be to log in using a username and password, or just to accept the network's terms and conditions.

There are many different ways in which captive portal network might attempt to direct you to the captive portal page.
- A DNS resolver that always resolves to the captive portal server IP
- A gateway that intercepts all HTTP requests and responds with a 302/307 redirect to the captive portal page
- A gateway that rewrites all/specific HTTP responses
    - Changing their content to be that of the captive portal page
    - Injecting javascript or other content into the page (Some ISPs do this when the user hasn't paid their internet bill)
- HTTPS requests are handled differently by captive portals:
    - They might time out.
    - They might present the wrong certificate in order to redirect to the captive portal.
    - They might not be intercepted at all.

## Implementation
The [CaptivePortalService](https://searchfox.org/mozilla-central/source/netwerk/base/CaptivePortalService.h) controls when the checks are performed. Consumers can check the state on [nsICaptivePortalService](https://searchfox.org/mozilla-central/source/netwerk/base/nsICaptivePortalService.idl) to determine the state of the captive portal.
- UNKNOWN
    - The checks have not been performed or have timed out.
- NOT_CAPTIVE
    - No captive portal interference was detected.
- UNLOCKED_PORTAL
    - A captive portal was previously detected, but has been unlocked by the user. This state might cause the browser to increase the frequency of the captive portal checks.
- LOCKED_PORTAL
    - A captive portal was detected, and internet connectivity is not currently available.
    - A [captive portal notification bar](https://searchfox.org/mozilla-central/source/browser/base/content/browser-captivePortal.js) might be displayed to the user.

The Captive portal service uses [CaptiveDetect.sys.mjs](https://searchfox.org/mozilla-central/source/toolkit/components/captivedetect/CaptiveDetect.jsm) to perform the checks, which in turn uses XMLHttpRequest.
This request needs to be exempted from HTTPS upgrades, DNS over HTTPS, and many new browser features in order to function as expected.

```{note}

CaptiveDetect.sys.mjs would benefit from being rewritten in rust or C++.
This is because the API of XMLHttpRequest makes it difficult to distinguish between different types of network errors such as redirect loops vs certificate errors.

Also, we don't currently allow any redirects to take place, even if the redirected resource acts as a transparent proxy (doesn't modify the response). This sometimes causes issues for users on networks which employ such transparent proxies.

```

## Preferences
```js

pref("network.captive-portal-service.enabled", false); // controls if the checking is performed
pref("network.captive-portal-service.minInterval", 60000); // 60 seconds
pref("network.captive-portal-service.maxInterval", 1500000); // 25 minutes
// Every 10 checks, the delay is increased by a factor of 5
pref("network.captive-portal-service.backoffFactor", "5.0");

// The URL used to perform the captive portal checks
pref("captivedetect.canonicalURL", "http://detectportal.firefox.com/canonical.html");
// The response we expect to receive back for the canonical URL
// It contains valid HTML that when loaded in a browser redirects the user
// to a support page explaining captive portals.
pref("captivedetect.canonicalContent", "<meta http-equiv=\"refresh\" content=\"0;url=https://support.mozilla.org/kb/captive-portal\"/>");

// The timeout for each request.
pref("captivedetect.maxWaitingTime", 5000);
// time to retrigger a new request
pref("captivedetect.pollingTime", 3000);
// Number of times to retry the captive portal check if there is an error or timeout.
pref("captivedetect.maxRetryCount", 5);

```


# Connectivity checking
We use a mechanism similar to captive portal checking to verify if the browser has internet connectivity. The [NetworkConnectivityService](https://searchfox.org/mozilla-central/source/netwerk/base/NetworkConnectivityService.h) will periodically connect to the same URL we use for captive portal detection, but will restrict its preferences to either IPv4 or IPv6. Based on which responses succeed, we can infer if Firefox has IPv4 and/or IPv6 connectivity. We also perform DNS queries to check if the system has a IPv4/IPv6 capable DNS resolver.

## Preferences

```js

pref("network.connectivity-service.enabled", true);
pref("network.connectivity-service.DNSv4.domain", "example.org");
pref("network.connectivity-service.DNSv6.domain", "example.org");
pref("network.connectivity-service.IPv4.url", "http://detectportal.firefox.com/success.txt?ipv4");
pref("network.connectivity-service.IPv6.url", "http://detectportal.firefox.com/success.txt?ipv6");

```
