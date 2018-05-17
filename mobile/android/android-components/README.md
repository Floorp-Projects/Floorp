# Android components

[![Task Status](https://github.taskcluster.net/v1/repository/mozilla-mobile/android-components/master/badge.svg)](https://github.taskcluster.net/v1/repository/mozilla-mobile/android-components/master/latest)
[![Build Status](https://travis-ci.org/mozilla-mobile/android-components.svg?branch=master)](https://travis-ci.org/mozilla-mobile/android-components)
[![codecov](https://codecov.io/gh/mozilla-mobile/android-components/branch/master/graph/badge.svg)](https://codecov.io/gh/mozilla-mobile/android-components)
[![Sputnik](https://sputnik.ci/conf/badge)](https://sputnik.ci/app#/builds/mozilla-mobile/android-components)
![](https://api.bintray.com/packages/pocmo/Mozilla-Mobile/errorpages/images/download.svg)

_A collection of Android libraries to build browsers or browser-like applications._

# Getting Involved

We encourage you to participate in this open source project. We love Pull Requests, Bug Reports, ideas, (security) code reviews or any kind of positive contribution. Please read the [Community Participation Guidelines](https://www.mozilla.org/en-US/about/governance/policies/participation/).

# Components

* 🔴 **In Development** - Not ready to be used in shipping products.
* ⚪ **Preview** - This component is almost ready and can be (partially) tested in products.
* 🔵 **Production ready** - Used by shipping products.

## Browser

High-level components for building browser(-like) apps.

* 🔵 **Domains** Localized and customizable domain lists for auto-completion in browsers.

* 🔴 **Engine-Gecko** - *Engine* implementation based on [GeckoView](https://wiki.mozilla.org/Mobile/GeckoView).

* 🔴 **Engine-System** - *Engine* implementation based on the system's WebView.

* ⚪ **Erropages** - Responsive browser error pages for Android apps.

* 🔴 **Menu** - A generic menu with customizable items primarily for browser toolbars.

* 🔵 **Search** - Search plugins and companion code to load, parse and use them.

* 🔴 **Session** - A generic representation of a browser session.

* 🔴 **Toolbar** - A customizable toolbar for browsers.

## Concept

_API contracts and abstraction layers for browser components._

* 🔴 **Engine** - Abstraction layer that allows hiding the actual browser engine implementation.

* 🔴 **Session-Storage** - Abstraction layer and contracts for hiding the actual session storage implementation.

* 🔴 **Toolbar** - Abstract definition of a browser toolbar component.

## Feature

_Combined components to implement feature-specific use cases._

* 🔴 **Search** - A component that connects an (concept) engine implementation with the browser search module.

* 🔴 **Session** - A component that connects an (concept) engine implementation with the browser session module.

* 🔴 **Toolbar** - A component that connects a (concept) toolbar implementation with the browser session module.

## UI

_Generic low-level UI components for building apps._

* 🔵 **Autocomplete** - A set of components to provide autocomplete functionality.

* 🔵 **Colors** - The standard set of [Photon](https://design.firefox.com/photon/) colors.

* 🔵 **Fonts** - The standard set of fonts used by Mozilla Android products.

* 🔵 **Icons** - A collection of often used browser icons.

* 🔵 **Progress** - An animated progress bar following the Photon Design System. 

## Service

_Components and libraries to interact with backend services._

* 🔵 **Telemetry** - A generic library for sending telemetry pings from Android applications to Mozilla's telemetry service.

## Support

_Supporting components with generic helper code._

* 🔵 **Ktx** - A set of Kotlin extensions on top of the Android framework and Kotlin standard library.

* 🔵 **Utils** - Generic utility classes to be shared between projects.

# License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
