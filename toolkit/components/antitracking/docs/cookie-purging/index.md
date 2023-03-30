# Cookie Purging

“Cookie Purging” describes a technique that will periodically clear
cookies and site data of known tracking domains without user interaction
to protect against [bounce
tracking](https://privacycg.github.io/nav-tracking-mitigations/#bounce-tracking).

## Protection Background

### What similar protections do other browsers have?

**Safari** classifies sites as redirect trackers which directly or
shortly after navigation redirect the user to other sites. Sites which
receive user interaction are exempt from this. To detect bigger redirect
networks, sites may also inherit redirect tracker
[classification](https://privacycg.github.io/nav-tracking-mitigations/#mitigations-safari).
If a site is classified as a redirect tracker, any site pointing to it
will inherit this classification. Safari does not use tracker lists.

When the source site is classified as a tracker, Safari will purge all
storage, excluding cookies. Sites which receive user interaction within
seven days of browser use are exempt. If the destination site's URL
includes query parameters or URL fragments, Safari caps the lifetime of
client-side set cookies of the destination site to 24 hours.

**Brave** uses lists to classify redirect trackers. Recently, they have
rolled out a new protection, [Unlinkable Bouncing](https://brave.com/privacy-updates/16-unlinkable-bouncing/),
which limits first party storage lifetime. The underlying mechanism is
called “first-party ephemeral storage”. If a user visits a known
bounce-tracker which doesn’t have any pre-existing storage, the browser
will create a temporary first-party storage bucket for the destination
site. This temporary storage is cleared 30 seconds after the user closes
the last tab of the site.

**Chrome** and **Edge** currently do not implement any navigational
tracking protections.

### Is it standardized?

At this time there are no standardized navigational tracking
protections. The PrivacyCG has a [work item for Navigation-based Tracking Mitigations](https://privacycg.github.io/nav-tracking-mitigations/).
Also see Apple’s proposal
[here](https://github.com/privacycg/proposals/issues/6).

### How does it fit into our vision of “Zero Privacy Leaks?”

Existing tracking protections mechanisms focus mostly on third-party
trackers. Redirect tracking can circumvent these mechanisms and utilize
first-party storage for tracking. Cookie purging contributes to the
“Zero Privacy Leaks” vision by mitigating this cross-site tracking
vector.

## Firefox Status

Metabug: [Bug 1594226 - \[Meta\] Purging Tracking Cookies](https://bugzilla.mozilla.org/show_bug.cgi?id=1594226)

### What is the ship state of this protection in Firefox?

Shipped to Release in standard ETP mode

### Is there outstanding work?

The mechanism of storing user interaction as a permission via
nsIPermissionManager has shown to be brittle and has led to users
getting logged out of sites in the past. The concept of a permission
doesn’t fully match that of a user interaction flag. Permissions may be
separated between normal browsing and PBM (Bug
[1692567](https://bugzilla.mozilla.org/show_bug.cgi?id=1692567)).
They may also get purged when clearing site data (Bug
[1675018](https://bugzilla.mozilla.org/show_bug.cgi?id=1675018)).

A proposed solution to this is to create a dedicated data store for
keeping track of user interaction. This could also enable tracking user
interaction relative to browser usage time, rather than absolute time
([Bug 1637146](https://bugzilla.mozilla.org/show_bug.cgi?id=1637146)).

Important outstanding bugs:
-   [Bug 1637146 - Use use-time rather than absolute time when computing whether to purge cookies](https://bugzilla.mozilla.org/show_bug.cgi?id=1637146)

### Existing Documentation

-   [https://developer.mozilla.org/en-US/docs/Web/Privacy/Redirect\_tracking\_protection](https://developer.mozilla.org/en-US/docs/Web/Privacy/Redirect_tracking_protection)

-   [PrivacyCG: Navigational-Tracking Mitigations](https://privacycg.github.io/nav-tracking-mitigations/)


## Technical Information

### Feature Prefs

Cookie purging can be enabled or disabled by flipping the
`privacy.purge_trackers.enabled` preference. Further, it will only run if
the `network.cookie.cookieBehavior` pref is set to `4` or `5` ([bug 1643045](https://bugzilla.mozilla.org/show_bug.cgi?id=1643045) adds
support for behaviors `1` and `3`).

Different log levels can be set via the pref
`privacy.purge_trackers.logging.level`.

The time until user interaction permissions expire can be set to a lower
amount of time using the `privacy.userInteraction.expiration` pref. Note
that you will have to set this pref before visiting the sites you want
to test on, it will not apply retroactively.

### How does it work?

Cookie purging periodically clears first-party storage of known
trackers, which the user has not interacted with recently. It is
implemented in the
[PurgeTrackerService](https://searchfox.org/mozilla-central/rev/cf77e656ef36453e154bd45a38eea08b13d6a53e/toolkit/components/antitracking/PurgeTrackerService.jsm),
which implements the
[nsIPurgeTrackerService](https://searchfox.org/mozilla-central/rev/cf77e656ef36453e154bd45a38eea08b13d6a53e/toolkit/components/antitracking/nsIPurgeTrackerService.idl)
IDL interface.

#### What origins are cleared?

An origin will be cleared if it fulfills the following conditions:

1.  It has stored cookies or accessed other site storage (e.g.
    [localStorage](https://developer.mozilla.org/en-US/docs/Web/API/Web_Storage_API),
    [IndexedDB](https://developer.mozilla.org/en-US/docs/Web/API/IndexedDB_API),
    or the [Cache API](https://developer.mozilla.org/en-US/docs/Web/API/CacheStorage))
    within the last 72 hours. Since cookies are per-host, we will
    clear both the http and https origin variants of a cookie host.

2.  The origin is [classified as a tracker](https://developer.mozilla.org/en-US/docs/Web/Privacy/Storage_Access_Policy#tracking_protection_explained)
    in our Tracking Protection list.

3.  No origin with the same base domain (eTLD+1) has a user-interaction
    permission.

    -   This permission is granted to an origin for 45 days once a user
        interacts with a top-level document from that origin.
        "Interacting" includes scrolling.

    -   Although this permission is stored on a per-origin level, we
        will check whether any origin with the same base domain has
        it, to avoid breaking sites with subdomains and a
        corresponding cookie setup.

#### What data is cleared?

Firefox will clear the [following data](https://searchfox.org/mozilla-central/rev/cf77e656ef36453e154bd45a38eea08b13d6a53e/toolkit/components/antitracking/PurgeTrackerService.jsm#205-213):

-   Network cache and image cache

-   Cookies

-   DOM Quota Storage (localStorage, IndexedDB, ServiceWorkers, DOM
    Cache, etc.)

-   DOM Push notifications

-   Reporting API Reports

-   Security Settings (i.e. HSTS)

-   EME Media Plugin Data

-   Plugin Data (e.g. Flash)

-   Media Devices

-   Storage Access permissions granted to the origin

-   HTTP Authentication Tokens

-   HTTP Authentication Cache

**Note:** Even though we're clearing all of this data, we currently only
flag origins for clearing when they use cookies or other site storage.

Storage clearing ignores origin attributes. This means that storage will
be cleared across
[containers](https://wiki.mozilla.org/Security/Contextual_Identity_Project/Containers)
and isolated storage (i.e. from [First-Party Isolation](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/cookies#first-party_isolation)).

#### How frequently is data cleared?

Firefox clears storage based on the firing of an internal event called
[idle-daily](https://searchfox.org/mozilla-central/rev/cf77e656ef36453e154bd45a38eea08b13d6a53e/toolkit/components/antitracking/PurgeTrackerService.jsm#60,62,65),
which is defined by the following conditions:

-   It will, at the earliest, fire 24h after the last idle-daily event
    fired.

-   It will only fire if the user has been idle for at least 3min (for
    24-48h after the last idle-daily) or 1 min (for &gt;48h after the
    last idle-daily).

This means that there are at least 24 hours between each storage
clearance, and storage will only be cleared when the browser is idle.
When clearing cookies, we sort cookies by creation date and batch them
into sets of 100 (controlled by the pref
`privacy.purge_trackers.max_purge_count`) for performance reasons.

#### Debugging

For debugging purposes, it's easiest to trigger storage clearing by
triggering the service directly via the [Browser Console command line](/devtools-user/browser_console/index.rst#browser_console_command_line).
Note that this is different from the normal [Web Console](/devtools-user/web_console/index.rst)
you might use to debug a website, and requires the
`devtools.chrome.enabled` pref to be set to true to use it interactively.
Once you've enabled the Browser Console you can trigger storage clearing
by running the following command:

``` javascript
await Components.classes["@mozilla.org/purge-tracker-service;1"]
.getService(Components.interfaces.nsIPurgeTrackerService)
.purgeTrackingCookieJars()
```

<!---
TODO: consider integrating
[https://developer.mozilla.org/en-US/docs/Web/Privacy/Redirect\_tracking\_protection](https://developer.mozilla.org/en-US/docs/Web/Privacy/Redirect_tracking_protection)
into firefox source docs. The article doesn’t really belong into MDN,
because it’s very specific to Firefox.
-->
