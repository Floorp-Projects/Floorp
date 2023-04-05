# Firefox Focus for Android

_Browse like no one’s watching. The new Firefox Focus automatically blocks a wide range of online trackers — from the moment you launch it to the second you leave it. Easily erase your history, passwords and cookies, so you won’t get followed by things like unwanted ads._

Firefox Focus provides automatic ad blocking and tracking protection on an easy-to-use private browser.

<a href="https://play.google.com/store/apps/details?id=org.mozilla.focus" target="_blank"><img src="https://play.google.com/intl/en_us/badges/images/generic/en-play-badge.png" alt="Get it on Google Play" height="90"/></a>

* [Google Play: Firefox Focus (Global)](https://play.google.com/store/apps/details?id=org.mozilla.focus)
* [Google Play: Firefox Klar (Germany, Austria & Switzerland)](https://play.google.com/store/apps/details?id=org.mozilla.klar)
* [Download APKs](https://github.com/mozilla-mobile/focus-android/releases)



## Getting Involved


We encourage you to participate in this open source project. We love Pull Requests, Bug Reports, ideas, (security) code reviews or any other kind of positive contribution.

Before you attempt to make a contribution please read the [Community Participation Guidelines](https://www.mozilla.org/en-US/about/governance/policies/participation/).

* [Guide to Contributing](https://github.com/mozilla-mobile/shared-docs/blob/main/android/CONTRIBUTING.md) (**New contributors start here!**)

* [View current Issues](https://github.com/mozilla-mobile/focus-android/issues), [view current Pull Requests](https://github.com/mozilla-mobile/focus-android/pulls), or [file a security issue][sec issue].

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
  ./gradlew clean app:assembleFocusDebug
  ```

3. Make sure to select the correct build variant in Android Studio:
**focusArmDebug** for ARM
**focusX86Debug** for X86
**focusAarch64Debug** for ARM64

## local.properties helpers
You can speed up or enhance local development by setting a few helper flags available in `local.properties` which will be made easily available as gradle properties.

### Automatically sign release builds
To sign your release builds with your debug key automatically, add the following to `<proj-root>/local.properties`:

```sh
autosignReleaseWithDebugKey
```

With this line, release build variants will automatically be signed with your debug key (like debug builds), allowing them to be built and installed directly through Android Studio or the command line.

This is helpful when you're building release variants frequently, for example to test feature flags and or do performance analyses.

### Building debuggable release variants

Nightly, Beta and Release variants are getting published to Google Play and therefore are not debuggable. To locally create debuggable builds of those variants, add the following to `<proj-root>/local.properties`:

```sh
debuggable
```

### Auto-publication workflow for application-services and glean
If you're making changes to these projects and want to test them in Focus, auto-publication workflow is the fastest, most reliable
way to do that.

In `local.properties`, specify a relative path to your local `glean` and/or `application-services` projects. E.g.:
- `autoPublish.glean.dir=../glean`
- `autoPublish.application-services.dir=../application-services`

Once these flags are set, your Focus builds will include any local modifications present in these projects.

See a [demo of auto-publication workflow in action](https://www.youtube.com/watch?v=qZKlBzVvQGc).

## Pre-push hooks
To reduce review turn-around time, we'd like all pushes to run tests locally. We'd
recommend you use our provided pre-push hook in `quality/pre-push-recommended.sh`.
Using this hook will guarantee your hook gets updated as the repository changes.
This hook tries to run as much as possible without taking too much time.

To add it, run this command from the project root:
```sh
ln -s ../../quality/pre-push-recommended.sh .git/hooks/pre-push
```

To push without running the pre-push hook (e.g. doc updates):
```sh
git push <remote> --no-verify
```

## Test Channel on Google PlayStore
To get Focus Nightly on your device, follow these steps:

1) Visit https://groups.google.com/g/firefox-focus-pre-release and join the Google Group
2) After you have joined the group opt-in to receive Nightly builds, again with the same Google account: https://play.google.com/apps/testing/org.mozilla.focus.nightly
3) Download Firefox Focus (Nightly) from Google Play: https://play.google.com/store/apps/details?id=org.mozilla.focus.nightly

Make sure you use the same Google Account for both steps.


## License


    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/

[sec issue]: https://bugzilla.mozilla.org/enter_bug.cgi?assigned_to=nobody%40mozilla.org&bug_file_loc=http%3A%2F%2F&bug_ignored=0&bug_severity=normal&bug_status=NEW&cf_fx_iteration=---&cf_fx_points=---&component=Security%3A%20Android&contenttypemethod=autodetect&contenttypeselection=text%2Fplain&defined_groups=1&flag_type-4=X&flag_type-607=X&flag_type-791=X&flag_type-800=X&flag_type-803=X&form_name=enter_bug&groups=firefox-core-security&maketemplate=Remember%20values%20as%20bookmarkable%20template&op_sys=Unspecified&priority=--&product=Focus&rep_platform=Unspecified&target_milestone=---&version=---
