# Connectivity Checking
We use a mechanism similar to captive portal checking to verify if the browser has internet connectivity. The [NetworkConnectivityService](https://searchfox.org/mozilla-central/source/netwerk/base/NetworkConnectivityService.h) will periodically connect to the same URL we use for captive portal detection, but will restrict its preferences to either IPv4 or IPv6. Based on which responses succeed, we can infer if Firefox has IPv4 and/or IPv6 connectivity. We also perform DNS queries to check if the system has a IPv4/IPv6 capable DNS resolver.

## Preferences

```js

pref("network.connectivity-service.enabled", true);
pref("network.connectivity-service.DNSv4.domain", "example.org");
pref("network.connectivity-service.DNSv6.domain", "example.org");
pref("network.connectivity-service.IPv4.url", "http://detectportal.firefox.com/success.txt?ipv4");
pref("network.connectivity-service.IPv6.url", "http://detectportal.firefox.com/success.txt?ipv6");

```
