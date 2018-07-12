# Firefox Focus for Android

[![Build Status](https://travis-ci.org/mozilla-mobile/focus-android.svg?branch=master)](https://travis-ci.org/mozilla-mobile/focus-android)
[![Task Status](https://github.taskcluster.net/v1/repository/mozilla-mobile/focus-android/master/badge.svg)](https://github.taskcluster.net/v1/repository/mozilla-mobile/focus-android/master/latest)
[![codecov](https://codecov.io/gh/mozilla-mobile/focus-android/branch/master/graph/badge.svg)](https://codecov.io/gh/mozilla-mobile/focus-android/branch/master)
[![Sputnik](https://sputnik.ci/conf/badge)](https://sputnik.ci/app#/builds/mozilla-mobile/focus-android)


_Browse like no one’s watching. The new Firefox Focus automatically blocks a wide range of online trackers — from the moment you launch it to the second you leave it. Easily erase your history, passwords and cookies, so you won’t get followed by things like unwanted ads._ 

Firefox Focus provides automatic ad blocking and tracking protection on an easy-to-use private browser.

<a href="https://play.google.com/store/apps/details?id=org.mozilla.focus" target="_blank"><img src="https://play.google.com/intl/en_us/badges/images/generic/en-play-badge.png" alt="Get it on Google Play" height="90"/></a>

* [Google Play: Firefox Focus (Global)](https://play.google.com/store/apps/details?id=org.mozilla.focus)
* [Google Play: Firefox Klar (Germany, Austria & Switzerland)](https://play.google.com/store/apps/details?id=org.mozilla.klar)
* [Download APKs](https://github.com/mozilla-mobile/focus-android/releases)

## Getting Involved


We encourage you to participate in this open source project. We love Pull Requests, Bug Reports, ideas, (security) code reviews or any other kind of positive contribution. 

Before you attempt to make a contribution please read the [Community Participation Guidelines](https://www.mozilla.org/en-US/about/governance/policies/participation/).

* [Guide to Contributing](https://github.com/mozilla-mobile/focus-android/wiki/Contributing) (**New contributors start here!**)

* [View current Issues](https://github.com/mozilla-mobile/focus-android/issues) or [View current Pull Requests](https://github.com/mozilla-mobile/focus-android/pulls).

* IRC: [#focus (irc.mozilla.org)](https://wiki.mozilla.org/IRC) | [view logs](https://mozilla.logbot.info/focus/)
(**We're available Monday-Friday, GMT and PST working hours**).

* Opt-in to our Mailing List [firefox-focus-public@](https://mail.mozilla.org/listinfo/firefox-focus-public) to keep up to date.

* [View the Wiki](https://github.com/mozilla-mobile/focus-android/wiki).

**Beginners!** - Watch out for [Issues with the "Good First Issue" label](https://github.com/mozilla-mobile/focus-android/issues?q=is%3Aopen+is%3Aissue+label%3A%22good+first+issue%22). These are easy bugs that have been left for first timers to have a go, get involved and make a positive contribution to the project!

## Build Instructions


1. Clone or Download the repository:

  ```shell
  git clone https://github.com/mozilla-mobile/focus-android
  ```

2. Import the project into Android Studio **or** build on the command line:

  ```shell
  ./gradlew clean app:assembleFocusWebviewArmDebug
  ```

3. Make sure to select the correct build variant in Android Studio:
**focusWebviewArmDebug** for ARM
**focusWebviewX86Debug** for X86
**focusWebviewAarch64Debug** for ARM64

## License


    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
