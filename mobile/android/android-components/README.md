# Android components

[![Task Status](https://github.taskcluster.net/v1/repository/mozilla-mobile/android-components/master/badge.svg)](https://github.taskcluster.net/v1/repository/mozilla-mobile/android-components/master/latest)
[![Build Status](https://travis-ci.org/mozilla-mobile/android-components.svg?branch=master)](https://travis-ci.org/mozilla-mobile/android-components)
[![codecov](https://codecov.io/gh/mozilla-mobile/android-components/branch/master/graph/badge.svg)](https://codecov.io/gh/mozilla-mobile/android-components)
[![Sputnik](https://sputnik.ci/conf/badge)](https://sputnik.ci/app#/builds/mozilla-mobile/android-components)
![](https://api.bintray.com/packages/pocmo/Mozilla-Mobile/errorpages/images/download.svg)

_A collection of Android libraries to build browsers or browser-like applications._

# Getting Involved

We encourage you to participate in this open source project. We love pull requests, bug reports, ideas, (security) code reviews or any kind of positive contribution.

Before you attempt to make a contribution please read the [Community Participation Guidelines](https://www.mozilla.org/en-US/about/governance/policies/participation/).

* [View current Issues](https://github.com/mozilla-mobile/android-components/issues) or [View current Pull Requests](https://github.com/mozilla-mobile/android-components/pulls).

* [List of good first issues](https://github.com/mozilla-mobile/android-components/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22) (**New contributors start here!**) and [List of "help wanted" issues](https://github.com/mozilla-mobile/android-components/issues?q=is%3Aissue+is%3Aopen+label%3A%22help+wanted%22).

* IRC: [#android-components (irc.mozilla.org)](https://wiki.mozilla.org/IRC) | [view logs](https://mozilla.logbot.info/android-components/)

* Subscribe to our mailing list [android-components@](https://lists.mozilla.org/listinfo/android-components) to keep up to date ([Archives](https://lists.mozilla.org/pipermail/android-components/)).

# Architecture and Overview

Our main design goal is to provide independently reusable Android components. We strive to keep dependencies between components as minimal as possible. However, standalone components aren't always feasible, which is why we have grouped components based on their interactions and dependencies.

On the lowest level, we provide standalone UI components (e.g. autocomplete, progressbar, colors) as well as independent service and suppport libraries (e.g. Telemetry, Kotlin extensions and Utilities).

The second level consist of so called `Concept` modules. These are abstractions that describe contracts for component implementations such as `Engine` or `Session Storage`, which may in turn have multiple implementations. The purpose of these concepts is to allow for customization and pluggability. Therefore, where available, components should always depend on concept modules (not their implementations) to support bringing in alternative implementations.

On top of `Concept` modules we provide `Browser` components. These components provide browser-specific functionality by implementing concepts and using lower level components.

On the highest level, we provide `Feature` components which provide use case implementations (e.g search, load URL). Features can connect multiple `Browser` components with concepts, and will therefore depend on other components.

The following diagram does not contain all available components. See [Components](#components) for a complete and up-to-date list.

```
    ┌─────────────────────┬───────────────────────────────────────────────────────────────────────┐
    │                     │ ┌───────────────────────────────────────────────────────────────────┐ │
    │                     │ │                              Feature                              │ │
    │  Features combine   │ │ ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─    ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─   ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─  │ │
    │ browser components  │ │       Session      │         Toolbar      │         Search      │ │ │
    │    with concepts    │ │ └ ─ ─ ─ ─ ─ ─ ─ ─ ─    └ ─ ─ ─ ─ ─ ─ ─ ─ ─   └ ─ ─ ─ ─ ─ ─ ─ ─ ─  │ │
    │                     │ └───────────────────────────────────────────────────────────────────┘ │
    ├─────────────────────┼───────────────────────────────────────────────────────────────────────┤
    │                     │ ┌───────────────────────────────────────────────────────────────────┐ │
    │                     │ │                              Browser                              │ │
    │ Browser components  │ │ ┌ ─ ─ ─ ─ ─ ─ ┐ ┌ ─ ─ ─ ─ ─ ─ ┐  ┌ ─ ─ ─ ─ ─ ─ ┐  ┌ ─ ─ ─ ─ ─ ─ ┐ │ │
    │   may implement     │ │  Engine-Gecko       Search           Toolbar        Errorpages    │ │
    │  concepts and use   │ │ └ ─ ─ ─ ─ ─ ─ ┘ └ ─ ─ ─ ─ ─ ─ ┘  └ ─ ─ ─ ─ ─ ─ ┘  └ ─ ─ ─ ─ ─ ─ ┘ │ │
    │    lower level      │ │ ┌ ─ ─ ─ ─ ─ ─ ┐ ┌ ─ ─ ─ ─ ─ ─ ┐  ┌ ─ ─ ─ ─ ─ ─ ┐  ┌ ─ ─ ─ ─ ─ ─ ┐ │ │
    │     components      │ │  Engine-System      Session          Domains           Menu       │ │
    │                     │ │ └ ─ ─ ─ ─ ─ ─ ┘ └ ─ ─ ─ ─ ─ ─ ┘  └ ─ ─ ─ ─ ─ ─ ┘  └ ─ ─ ─ ─ ─ ─ ┘ │ │
    │                     │ └───────────────────────────────────────────────────────────────────┘ │
    ├─────────────────────┼───────────────────────────────────────────────────────────────────────┤
    │                     │ ┌───────────────────────────────────────────────────────────────────┐ │
    │  Abstractions and   │ │                              Concept                              │ │
    │   contracts for     │ │    ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─    ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │
    │     component       │ │               Engine          │             Toolbar          │    │ │
    │  implementations    │ │    └ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─    └ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │
    │                     │ └───────────────────────────────────────────────────────────────────┘ │
    ├─────────────────────├───────────────────────────────────────────────────────────────────────┤
    │                     │ ┌────────────────────────────────┐ ┌────────────────────────────────┐ │
    │                     │ │               UI               │ │            Service             │ │
    │     Standalone      │ │    ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │    ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │
    │     components      │ │          Autocomplete     │    │ │           Telemetry       │    │ │
    │                     │ │    └ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │    └ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │
    │                     │ │    ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ └────────────────────────────────┘ │
    │                     │ │            Progress       │    │ ┌────────────────────────────────┐ │
    │                     │ │    └ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │            Support             │ │
    │                     │ │    ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │    ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │
    │                     │ │             Colors        │    │ │       Kotlin extensions   │    │ │
    │                     │ │    └ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │    └ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │
    │                     │ │    ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │    ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │
    │                     │ │             Fonts         │    │ │           Utilities       │    │ │
    │                     │ │    └ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │    └ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │
    │                     │ │    ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │                                │ │
    │                     │ │             Icons         │    │ │                                │ │
    │                     │ │    └ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │                                │ │
    │                     │ └────────────────────────────────┘ └────────────────────────────────┘ │
    └─────────────────────┴───────────────────────────────────────────────────────────────────────┘
```

# Components

* 🔴 **In Development** - Not ready to be used in shipping products.
* ⚪ **Preview** - This component is almost ready and can be (partially) tested in products.
* 🔵 **Production ready** - Used by shipping products.

## Browser

High-level components for building browser(-like) apps.

* 🔵 [**Domains**](components/browser/domains/README.md) Localized and customizable domain lists for auto-completion in browsers.

* 🔴 [**Engine-Gecko**](components/browser/engine-gecko/README.md) - *Engine* implementation based on [GeckoView](https://wiki.mozilla.org/Mobile/GeckoView) (Release channel).

* 🔴 [**Engine-Gecko-Beta**](components/browser/engine-gecko-beta/README.md) - *Engine* implementation based on [GeckoView](https://wiki.mozilla.org/Mobile/GeckoView) (Beta channel).

* 🔴 [**Engine-Gecko-Nightly**](components/browser/engine-gecko/README.md) - *Engine* implementation based on [GeckoView](https://wiki.mozilla.org/Mobile/GeckoView) (Nightly channel).

* 🔴 [**Engine-System**](components/browser/engine-system/README.md) - *Engine* implementation based on the system's WebView.

* ⚪ [**Erropages**](components/browser/errorpages/README.md) - Responsive browser error pages for Android apps.

* 🔴 [**Menu**](components/browser/menu/README.md) - A generic menu with customizable items primarily for browser toolbars.

* 🔵 [**Search**](components/browser/search/README.md) - Search plugins and companion code to load, parse and use them.

* 🔴 [**Session**](components/browser/session/README.md) - A generic representation of a browser session.

* 🔴 [**Tabstray**](components/browser/tabstray/README.md) - A customizable tabs tray for browsers.

* 🔴 [**Toolbar**](components/browser/toolbar/README.md) - A customizable toolbar for browsers.

## Concept

_API contracts and abstraction layers for browser components._

* 🔴 [**Engine**](components/concept/engine/README.md) - Abstraction layer that allows hiding the actual browser engine implementation.

* 🔴 [**Tabstray**](components/concept/tabstray/README.md) - Abstract definition of a tabs tray component.

* 🔴 [**Toolbar**](components/concept/toolbar/README.md) - Abstract definition of a browser toolbar component.

## Feature

_Combined components to implement feature-specific use cases._

* 🔴 [**Search**](components/feature/search/README.md) - A component that connects an (concept) engine implementation with the browser search module.

* 🔴 [**Session**](components/feature/session/README.md) - A component that connects an (concept) engine implementation with the browser session module.

* 🔴 [**Tabs**](components/feature/tabs/README.md) - A component that connects a trabs tray implementation with the session and toolbar modules.

* 🔴 [**Toolbar**](components/feature/toolbar/README.md) - A component that connects a (concept) toolbar implementation with the browser session module.

## UI

_Generic low-level UI components for building apps._

* 🔵 [**Autocomplete**](components/ui/autocomplete/README.md) - A set of components to provide autocomplete functionality.

* 🔵 [**Colors**](components/ui/colors/README.md) - The standard set of [Photon](https://design.firefox.com/photon/) colors.

* 🔵 [**Fonts**](components/ui/fonts/README.md) - The standard set of fonts used by Mozilla Android products.

* 🔵 [**Icons**](components/ui/icons/README.md) - A collection of often used browser icons.

* 🔵 [**Progress**](components/ui/progress/README.md) - An animated progress bar following the Photon Design System. 

## Service

_Components and libraries to interact with backend services._

* 🔴 [**Firefox Accounts (FxA)**](components/service/firefox-accounts/README.md) - A library for integrating with Firefox Accounts.

* 🔵 [**Telemetry**](components/service/telemetry/README.md) - A generic library for sending telemetry pings from Android applications to Mozilla's telemetry service.

## Support

_Supporting components with generic helper code._

* 🔵 [**Ktx**](components/support/ktx/README.md) - A set of Kotlin extensions on top of the Android framework and Kotlin standard library.

* ⚪ [**Test**](components/support/test/README.md) - A collection of helpers for testing components.

* 🔵 [**Utils**](components/support/utils/README.md) - Generic utility classes to be shared between projects.

# Sample apps

_Sample apps using various components._

* [**Browser**](samples/browser) - A simple browser composed from browser components.

* [**Firefox Accounts (FxA)**](samples/firefox-accounts) - A simple app demoing Firefox Accounts integration.

* [**Toolbar**](samples/toolbar) - An app demoing multiple customized toolbars using the [**browser-toolbar**](components/browser/toolbar/README.md) component.

# License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
