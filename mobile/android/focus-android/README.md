# Firefox Focus for Android

[![BuddyBuild](https://dashboard.buddybuild.com/api/statusImage?appID=584f67f6f3d6eb01000842d6&branch=master&build=latest)](https://dashboard.buddybuild.com/apps/584f67f6f3d6eb01000842d6/build/latest?branch=master)
[![Build Status](https://travis-ci.org/mozilla-mobile/focus-android.svg?branch=master)](https://travis-ci.org/mozilla-mobile/focus-android)
[![Task Status](https://github.taskcluster.net/v1/repository/mozilla-mobile/focus-android/master/badge.svg)](https://github.taskcluster.net/v1/repository/mozilla-mobile/focus-android/master/latest)
[![codecov](https://codecov.io/gh/mozilla-mobile/focus-android/branch/master/graph/badge.svg)](https://codecov.io/gh/mozilla-mobile/focus-android/branch/master)
[![Sputnik](https://sputnik.ci/conf/badge)](https://sputnik.ci/app#/builds/mozilla-mobile/focus-android)

_Browse like no one’s watching. The new Firefox Focus automatically blocks a wide range of online trackers — from the moment you launch it to the second you leave it. Easily erase your history, passwords and cookies, so you won’t get followed by things like unwanted ads._

<a href="https://play.google.com/store/apps/details?id=org.mozilla.focus" target="_blank"><img src="https://play.google.com/intl/en_us/badges/images/generic/en-play-badge.png" alt="Get it on Google Play" height="90"/></a>

* [Google Play: Firefox Focus (All countries)](https://play.google.com/store/apps/details?id=org.mozilla.focus)
* [Google Play: Firefox Klar (Germany, Austria, Switzerland)](https://play.google.com/store/apps/details?id=org.mozilla.klar)
* [Download APKs](https://github.com/mozilla-mobile/focus-android/releases)

Getting Involved
----------------

We encourage you to participate in this open source project. We love Pull Requests, Bug Reports, ideas, (security) code reviews or any kind of positive contribution. Please read the [Community Participation Guidelines](https://www.mozilla.org/en-US/about/governance/policies/participation/).

* Issues: [https://github.com/mozilla-mobile/focus-android/issues](https://github.com/mozilla-mobile/focus-android/issues)

* IRC: [#focus (irc.mozilla.org)](https://wiki.mozilla.org/IRC)

* Mailing list: [firefox-focus-public@](https://mail.mozilla.org/listinfo/firefox-focus-public)

* Wiki: [https://github.com/mozilla-mobile/focus-android/wiki](https://github.com/mozilla-mobile/focus-android/wiki)

Watch out for [issues with the "good first bug" label](https://github.com/mozilla-mobile/focus-android/issues?q=is%3Aopen+is%3Aissue+label%3A%22good+first+bug%22). Those are easy starter bugs that are available to work on!

Build instructions
------------------

1. Clone the repository:

  ```shell
  git clone https://github.com/mozilla-mobile/focus-android
  ```

2. Import the project into Android Studio or build on the command line:

  ```shell
  ./gradlew clean app:assembleFocusWebviewDebug
  ```

3. Make sure to select the right build variant in Android Studio: **focusWebviewDebug**

License
-------

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
