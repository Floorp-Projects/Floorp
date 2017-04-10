# Firefox Focus for Android

_Browse like no one’s watching. The new Firefox Focus automatically blocks a wide range of online trackers — from the moment you launch it to the second you leave it. Easily erase your history, passwords and cookies, so you won’t get followed by things like unwanted ads._

Getting Involved
----------------

We encourage you to participate in this open source project. We love Pull Requests, Bug Reports, ideas, (security) code reviews or any kind of positive contribution. Please read the [Community Participation Guidelines](https://www.mozilla.org/en-US/about/governance/policies/participation/).

* Issues: [https://github.com/mozilla-mobile/focus-android/issues](https://github.com/mozilla-mobile/focus-android/issues)

* IRC: [#mobile (irc.mozilla.org)](https://wiki.mozilla.org/IRC)

* Mailing list: [mobile-firefox-dev](https://mail.mozilla.org/listinfo/mobile-firefox-dev)

Build instructions
------------------

1. Clone the repository:

  ```shell
  git clone https://github.com/mozilla-mobile/focus-android
  ```

1. Import the project into Android Studio or build on the command line:

  ```shell
  ./gradlew clean app:assembleFocusWebkitDebug
  ```

1. Make sure to select the right build variant in Android Studio: **focusWebkitDebug**

Docs
----

* [Content blocking](docs/contentblocking.md)
* [Translations](docs/translations.md)
* [Search](docs/search.md)
* [Telemetry](docs/telemetry.md)
