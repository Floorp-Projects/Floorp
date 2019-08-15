# Android components

[![Task Status](https://github.taskcluster.net/v1/repository/mozilla-mobile/android-components/master/badge.svg)](https://github.taskcluster.net/v1/repository/mozilla-mobile/android-components/master/latest)
[![codecov](https://codecov.io/gh/mozilla-mobile/android-components/branch/master/graph/badge.svg)](https://codecov.io/gh/mozilla-mobile/android-components)
[![Bors enabled](https://bors.tech/images/badge_small.svg)](https://app.bors.tech/repositories/19637)

_A collection of Android libraries to build browsers or browser-like applications._

ℹ️ For more information **[see the website](https://mozilla-mobile.github.io/android-components/)**.

A full featured reference browser implementation based on the components can be found in the [reference-browser repository](https://github.com/mozilla-mobile/reference-browser).

# Getting Involved

We encourage you to participate in this open source project. We love pull requests, bug reports, ideas, (security) code reviews or any kind of positive contribution.

Before you attempt to make a contribution please read the [Community Participation Guidelines](https://www.mozilla.org/en-US/about/governance/policies/participation/).

* [View current Issues](https://github.com/mozilla-mobile/android-components/issues) or [View current Pull Requests](https://github.com/mozilla-mobile/android-components/pulls).

* [List of good first issues](https://github.com/mozilla-mobile/android-components/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22) (**New contributors start here!**) and [List of "help wanted" issues](https://github.com/mozilla-mobile/android-components/issues?q=is%3Aissue+is%3Aopen+label%3A%22help+wanted%22).

* IRC: [#android-components (irc.mozilla.org)](https://wiki.mozilla.org/IRC) | [view logs](https://mozilla.logbot.info/android-components/)

* Subscribe to our mailing list [android-components@](https://lists.mozilla.org/listinfo/android-components) to keep up to date ([Archives](https://lists.mozilla.org/pipermail/android-components/)).

* Localization happens on [Pontoon](https://pontoon.mozilla.org/projects/android-l10n/). Please get in touch with delphine (at) mozilla (dot) com directly for more information.

# Maven repository

All components are getting published on [maven.mozilla.org](https://maven.mozilla.org/).
To use them, you need to add the following to your projects top-level build file, in the `allprojects` block (see e.g. the [reference-browser](https://github.com/mozilla-mobile/reference-browser/blob/master/build.gradle)):

```groovy
repositories {
    maven {
       url "https://maven.mozilla.org/maven2"
    }
}
```

## Snapshot builds

Snapshots are build daily from the `master` branch and published on [snapshots.maven.mozilla.org](https://snapshots.maven.mozilla.org).

# API Reference

The API reference docs are available at [mozac.org/api/](https://mozac.org/api/).

# Components

* 🔴 **In Development** - Not ready to be used in shipping products.
* ⚪ **Preview** - This component is almost/partially ready and can be tested in products.
* 🔵 **Production ready** - Used by shipping products.

## Browser

High-level components for building browser(-like) apps.

* ⚪ [**Awesomebar**](components/browser/awesomebar/README.md) - A customizable [Awesome Bar](https://support.mozilla.org/en-US/kb/awesome-bar-search-firefox-bookmarks-history-tabs) implementation for browsers.

* 🔵 [**Domains**](components/browser/domains/README.md) Localized and customizable domain lists for auto-completion in browsers.

* ⚪ [**Engine-Gecko**](components/browser/engine-gecko/README.md) - *Engine* implementation based on [GeckoView](https://wiki.mozilla.org/Mobile/GeckoView) (Release channel).

* ⚪ [**Engine-Gecko-Beta**](components/browser/engine-gecko-beta/README.md) - *Engine* implementation based on [GeckoView](https://wiki.mozilla.org/Mobile/GeckoView) (Beta channel).

* ⚪ [**Engine-Gecko-Nightly**](components/browser/engine-gecko-nightly/README.md) - *Engine* implementation based on [GeckoView](https://wiki.mozilla.org/Mobile/GeckoView) (Nightly channel).

* 🔴 [**Engine-Servo**](components/browser/engine-servo/README.md) - *Engine* implementation based on the [Servo Browser Engine](https://servo.org/).

* ⚪ [**Engine-System**](components/browser/engine-system/README.md) - *Engine* implementation based on the system's WebView.

* 🔵 [**Errorpages**](components/browser/errorpages/README.md) - Responsive browser error pages for Android apps.

* 🔴 [**Icons**](components/browser/icons/README.md) - A component for loading and storing website icons (like [Favicons](https://en.wikipedia.org/wiki/Favicon)).

* ⚪ [**Menu**](components/browser/menu/README.md) - A generic menu with customizable items primarily for browser toolbars.

* 🔵 [**Search**](components/browser/search/README.md) - Search plugins and companion code to load, parse and use them.

* 🔵 [**Session**](components/browser/session/README.md) - A generic representation of a browser session.

* 🔴 [**Storage-Memory**](components/browser/storage-memory/README.md) - An in-memory implementation of browser storage.

* ⚪ [**Storage-Sync**](components/browser/storage-sync/README.md) - A syncable implementation of browser storage backed by [application-services' Places lib](https://github.com/mozilla/application-services).

* 🔴 [**Tabstray**](components/browser/tabstray/README.md) - A customizable tabs tray for browsers.

* ⚪ [**Toolbar**](components/browser/toolbar/README.md) - A customizable toolbar for browsers.

## Concept

_API contracts and abstraction layers for browser components._

* ⚪ [**Awesomebar**](components/concept/awesomebar/README.md) - An abstract definition of an awesome bar component.

* ⚪ [**Engine**](components/concept/engine/README.md) - Abstraction layer that allows hiding the actual browser engine implementation.

* ⚪ [**Fetch**](components/concept/fetch/README.md) - An abstract definition of an HTTP client for fetching resources.

* 🔴 [**Push**](components/concept/push/README.md) - An abstract definition of a push service component.

* ⚪ [**Storage**](components/concept/storage/README.md) - Abstract definition of a browser storage component.

* 🔴 [**Tabstray**](components/concept/tabstray/README.md) - Abstract definition of a tabs tray component.

* ⚪ [**Toolbar**](components/concept/toolbar/README.md) - Abstract definition of a browser toolbar component.

## Feature

_Combined components to implement feature-specific use cases._

* 🔴 [**Accounts**](components/feature/accounts/README.md) - A component that connects an FxaAccountManager from [service-firefox-accounts](components/service/firefox-accounts/README.md) with [feature-tabs](components/feature/tabs/README.md) in order to facilitate authentication flows.

* ⚪ [**Awesomebar**](components/feature/awesomebar/README.md) - A component that connects a [concept-awesomebar](components/concept/awesomebar/README.md) implementation to a [concept-toolbar](components/concept/toolbar/README.md) implementation and provides implementations of various suggestion providers.

* ⚪ [**Context Menu**](components/feature/contextmenu/README.md) - A component for displaying context menus when *long-pressing* web content.

* 🔴 [**Custom Tabs**](components/feature/customtabs/README.md) - A component for providing [Custom Tabs](https://developer.chrome.com/multidevice/android/customtabs) functionality in browsers.

* ⚪ [**Downloads**](components/feature/downloads/README.md) - A component to perform downloads using the [Android downloads manager](https://developer.android.com/reference/android/app/DownloadManager).

* 🔴 [**Intent**](components/feature/intent/README.md) - A component that provides intent processing functionality by combining various other feature modules.

* 🔴 [**Progressive Web Apps (PWA)**](components/feature/pwa/README.md) - A component that provides functionality for supporting Progressive Web Apps (PWA).

* 🔴 [**Reader View**](components/feature/readerview/README.md) - A component that provides Reader View functionality.

* ⚪ [**QR**](components/feature/qr/README.md) - A component that provides functionality for scanning QR codes.

* 🔴 [**Search**](components/feature/search/README.md) - A component that connects an (concept) engine implementation with the browser search module.

* 🔴 [**SendTab**](components/feature/sendtab/README.md) - A component for sending tabs to other devices with a registered FxA Account.

* ⚪ [**Session**](components/feature/session/README.md) - A component that connects an (concept) engine implementation with the browser session and storage modules.

* 🔴 [**Sync**](components/feature/sync/README.md) -A component that provides synchronization orchestration for groups of (concept) SyncableStore objects.

* 🔴 [**Tabs**](components/feature/tabs/README.md) - A component that connects a tabs tray implementation with the session and toolbar modules.

* 🔴 [**Tab Collections**](components/feature/tab-collections/README.md) - Feature implementation for saving, restoring and organizing collections of tabs.

* 🔴 [**Toolbar**](components/feature/toolbar/README.md) - A component that connects a (concept) toolbar implementation with the browser session module.

* ⚪ [**Prompts**](components/feature/prompts/README.md) - A component that will handle all the common prompt dialogs from web content.

* ⚪ [**Push**](components/feature/push/README.md) - A component that provides Autopush messages with help from a supported push service.

* ⚪ [**Find In Page**](components/feature/findinpage/README.md) - A component that provides an UI widget for [find in page functionality](https://support.mozilla.org/en-US/kb/search-contents-current-page-text-or-links).

* 🔴 [**Site Permissions**](components/feature/sitepermissions/README.md) - A feature for showing site permission request prompts.

* 🔴 [**Web Notifications**](components/feature/webnotifications/README.md) - A component for displaying web notifications.

## UI

_Generic low-level UI components for building apps._

* 🔵 [**Autocomplete**](components/ui/autocomplete/README.md) - A set of components to provide autocomplete functionality.

* 🔵 [**Colors**](components/ui/colors/README.md) - The standard set of [Photon](https://design.firefox.com/photon/) colors.

* 🔵 [**Fonts**](components/ui/fonts/README.md) - The standard set of fonts used by Mozilla Android products.

* 🔵 [**Icons**](components/ui/icons/README.md) - A collection of often used browser icons.

* ⚪ [**Tabcounter**](components/ui/tabcounter/README.md) - A button that shows the current tab count and can animate state changes.

## Service

_Components and libraries to interact with backend services._

* 🔵 [**Firefox Accounts (FxA)**](components/service/firefox-accounts/README.md) - A library for integrating with Firefox Accounts.

* 🔴 [**Firefox Sync - Logins**](components/service/sync-logins/README.md) - A library for integrating with Firefox Sync - Logins.

* 🔵 [**Fretboard**](components/service/fretboard/README.md) - An Android framework for segmenting users in order to run A/B tests and roll out features gradually.

* 🔴 [**Glean**](components/service/glean/README.md) - A client-side telemetry SDK for collecting metrics and sending them to Mozilla's telemetry service (eventually replacing [service-telemetry](components/service/telemetry/README.md)).

* 🔴 [**Experiments**](components/service/experiments/README.md) - An Android SDK for running experiments on user segments in multiple branches.

* ⚪ [**Location**](components/service/location/README.md) - A library for accessing Mozilla's and other location services.

* 🔴 [**Pocket**](components/service/pocket/README.md) - A library for communicating with the Pocket API.

* 🔵 [**Telemetry**](components/service/telemetry/README.md) - A generic library for sending telemetry pings from Android applications to Mozilla's telemetry service.

## Support

_Supporting components with generic helper code._

* 🔵 [**Android Test**](components/support/android-test/README.md) - A collection of helpers for testing components in instrumented (on device) tests (`src/androidTest`).

* 🔵 [**Base**](components/support/base/README.md) - Base component containing building blocks for components.

* 🔵 [**Ktx**](components/support/ktx/README.md) - A set of Kotlin extensions on top of the Android framework and Kotlin standard library.

* 🔵 [**Test**](components/support/test/README.md) - A collection of helpers for testing components in local unit tests (`src/test`).

* 🔵 [**Test Appservices**](components/support/test-appservices/README.md) - A component for synchronizing Application Services' unit testing dependencies used in Android Components.

* 🔵 [**Utils**](components/support/utils/README.md) - Generic utility classes to be shared between projects.

## Standalone libraries

* ⚪ [**Crash**](components/lib/crash/README.md) - A generic crash reporter component that can report crashes to multiple services.

* 🔵 [**Dataprotect**](components/lib/dataprotect/README.md) - A component using AndroidKeyStore to protect user data.

* ⚪ [**Fetch-HttpURLConnection**](components/lib/fetch-httpurlconnection/README.md) - A [concept-fetch](concept/fetch/README.md) implementation using [HttpURLConnection](https://developer.android.com/reference/java/net/HttpURLConnection.html).

* ⚪ [**Fetch-OkHttp**](components/lib/fetch-okhttp/README.md) - A [concept-fetch](concept/fetch/README.md) implementation using [OkHttp](https://github.com/square/okhttp).

* ⚪ [**JEXL**](components/lib/jexl/README.md) - Javascript Expression Language: Context-based expression parser and evaluator.

* ⚪ [**Public Suffix List**](components/lib/publicsuffixlist/README.md) - A library for reading and using the [public suffix list](https://publicsuffix.org/).

* ⚪ [**State**](components/lib/state/README.md) - A library for maintaining application state.

* 🔴 [**Push-Firebase**](components/lib/push-firebase/README.md) - A [concept-push](concept/push/README.md) implementation using [Firebase Cloud Messaging](https://firebase.google.com/products/cloud-messaging/).

## Tooling

* 🔵 [**Fetch-Tests**](components/tooling/fetch-tests/README.md) - A generic test suite for components that implement [concept-fetch](concept/fetch/README.md).

* 🔵 [**Lint**](components/tooling/lint/README.md) - Custom Lint rules for the components repository.

# Sample apps

_Sample apps using various components._

* [**Browser**](samples/browser) - A simple browser composed from browser components. This sample application is only a very basic browser. For a full-featured reference browser implementation see the **[reference-browser repository](https://github.com/mozilla-mobile/reference-browser)**.

* [**Crash**](samples/crash) - An app showing the integration of the `lib-crash` component.

* [**Firefox Accounts (FxA)**](samples/firefox-accounts) - A simple app demoing Firefox Accounts integration.

* [**Firefox Sync**](samples/sync) - A simple app demoing general Firefox Sync integration, with bookmarks and history.

* [**Firefox Sync - Logins**](samples/sync-logins) - A simple app demoing Firefox Sync (Logins) integration.

* [**Toolbar**](samples/toolbar) - An app demoing multiple customized toolbars using the [**browser-toolbar**](components/browser/toolbar/README.md) component.

* [**DataProtect**](samples/dataprotect) - An app demoing how to use the [**Dataprotect**](components/lib/dataprotect/README.md) component to load and store encrypted data in `SharedPreferences`.

* [**Glean**](samples/glean) - An app demoing how to use the [**Glean**](components/service/glean/README.md) library to collect and send telemetry data.

# License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
