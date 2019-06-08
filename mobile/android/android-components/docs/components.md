---
layout: page
title: Components
permalink: /components/
---

## Browser

### Core components

Every browser will need two core components:

* **[browser-session](https://github.com/mozilla-mobile/android-components/tree/master/components/browser/session)** - Representing the state of the browser (_"What tabs are opened?"_, _"What URLs are they pointing to?"_)
* **browser-engine** - The browser engine that transforms web pages into an interactive visual representation. The _browser-engine_ component comes in multiple flavors. We are supporting [Android's WebView](https://github.com/mozilla-mobile/android-components/tree/master/components/browser/engine-system) (limited feature set) and **GeckoView** ([Release](https://github.com/mozilla-mobile/android-components/tree/master/components/browser/engine-gecko), [Beta](https://github.com/mozilla-mobile/android-components/tree/master/components/browser/engine-gecko-beta) and [Nightly](https://github.com/mozilla-mobile/android-components/tree/master/components/browser/engine-gecko-nightly) channels) currently. The actual implementation is hidden behind [generic interfaces](https://github.com/mozilla-mobile/android-components/tree/master/components/concept) so that apps can build against multiple engines (e.g. based on product flavor) and so that other components work seemlessly with all implementations.

Other high-level browser components may depend on those core components.

### Building blocks

The following components offer customizable building blocks for browser apps:

* [browser-toolbar](https://github.com/mozilla-mobile/android-components/tree/master/components/browser/toolbar) - A customizable browser toolbar for displaying and editing URLs, actions buttons and menus.
* [browser-domains](https://github.com/mozilla-mobile/android-components/tree/master/components/browser/domains) - Localized and customizable domain lists for auto-completion in browsers.
* [browser-errorpages](https://github.com/mozilla-mobile/android-components/tree/master/components/browser/errorpages) - Localized and responsive error pages for mobile browsers.
* [browser-search](https://github.com/mozilla-mobile/android-components/tree/master/components/browser/search) - Localized search plugins and code for loading and parsing them as well as querying search engines for search suggestions.
* [browser-tabstray](https://github.com/mozilla-mobile/android-components/tree/master/components/browser/tabstray) - A customizable tabs tray component for mobile browsers.
* [browser-menu](https://github.com/mozilla-mobile/android-components/tree/master/components/browser/menu) - A customizable overflow menu. Can be used with _browser-toolbar_.

### Features

Feature components connect multiple components together and implement complete browser features.

[List of feature components in the android-components repository](https://github.com/mozilla-mobile/android-components/tree/master/components/feature)

## Apps

Components for Android apps - browsers and other apps.

### UI components

Independent, small visual UI elements to use in applications.

* [ui-autocomplete](https://github.com/mozilla-mobile/android-components/tree/master/components/ui/autocomplete) - A user interface element for entering and modifying text with the ability to inline autocomplete.
* [ui-colors](https://github.com/mozilla-mobile/android-components/tree/master/components/ui/colors) - The standard set of [colors](https://design.firefox.com/photon/visuals/color.html) used in the [Photon Design System](https://design.firefox.com/photon/) for Firefox products.
* [ui-fonts](https://github.com/mozilla-mobile/android-components/tree/master/components/ui/fonts) - The standard set of fonts used by Mozilla Android products.
* [ui-icons]({{ site.baseurl }}/components/ui/icons) - Android vector drawable versions of the [icons](https://design.firefox.com/icons/viewer/) from the [Photon Design System](https://design.firefox.com/photon/).
* [ui-progress](https://github.com/mozilla-mobile/android-components/tree/master/components/ui/progress) - An animated progress bar following the [Photon Design System](https://design.firefox.com/photon/)..
* [ui-tabcounter](https://github.com/mozilla-mobile/android-components/tree/master/components/ui/tabcounter) - A button that shows the current tab count and can animate state changes.

### Services

* [service-firefox-accounts](https://github.com/mozilla-mobile/android-components/tree/master/components/service/firefox-accounts) - A library for integrating with [Firefox Accounts](https://mozilla.github.io/application-services/docs/accounts/welcome.html).
* [service-fretboard](https://github.com/mozilla-mobile/android-components/tree/master/components/service/fretboard) - An Android framework for segmenting users in order to run A/B tests and rollout features gradually.
* [service-pocket](https://github.com/mozilla-mobile/android-components/tree/master/components/service/pocket) - A library for communicating with the Pocket API.
* [service-telemetry](https://github.com/mozilla-mobile/android-components/tree/master/components/service/telemetry) - A generic library for sending telemetry pings from Android applications to Mozilla's telemetry service.
* [service-glean](https://github.com/mozilla-mobile/android-components/tree/master/components/service/glean) - A generic library for sending telemetry pings from Android applications to Mozilla's telemetry service (eventually replacing `service-telemetry`).

## Samples

* [Reference Browser (full-featured browser)](https://github.com/mozilla-mobile/reference-browser) - Advanced reference browser implementation based on the browser components.
* [Simple Browser](https://github.com/mozilla-mobile/android-components/blob/master/samples/browser) - Very basic browser implementation based on the browser components.
* [Crash](https://github.com/mozilla-mobile/android-components/blob/master/samples/crash) - Sample integration for the [lib-crash](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/README.md) component.
* [DataProtect](https://github.com/mozilla-mobile/android-components/blob/master/samples/dataprotect) - An app demoing how to use the [Dataprotect](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/dataprotect/README.md) component to load and store encrypted data in SharedPreferences.
* [Firefox Accounts (FxA)](https://github.com/mozilla-mobile/android-components/blob/master/samples/firefox-accounts) - A simple app demoing Firefox Accounts integration.
* [Firefox Sync](https://github.com/mozilla-mobile/android-components/blob/master/samples/sync) - A simple app demoing general Firefox Sync integration, with bookmarks and history.
* [Firefox Sync - Logins](https://github.com/mozilla-mobile/android-components/blob/master/samples/sync-logins) - A simple app demoing Firefox Sync (Logins) integration.
* [Glean](https://github.com/mozilla-mobile/android-components/blob/master/samples/glean) - An app demoing how to use the [Glean](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/README.md) library to collect and send telemetry data.
* [Toolbar](https://github.com/mozilla-mobile/android-components/blob/master/samples/toolbar) - An app demoing multiple customized toolbars using the [browser-toolbar](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/toolbar/README.md) component.

## More

[List of all components in the android-components repository](https://github.com/mozilla-mobile/android-components/blob/master/README.md)
