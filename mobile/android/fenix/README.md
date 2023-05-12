# Firefox for Android

[![Task Status](https://firefox-ci-tc.services.mozilla.com/api/github/v1/repository/mozilla-mobile/fenix/main/badge.svg)](https://firefox-ci-tc.services.mozilla.com/api/github/v1/repository/mozilla-mobile/fenix/main/latest)
[![codecov](https://codecov.io/gh/mozilla-mobile/fenix/branch/main/graph/badge.svg)](https://codecov.io/gh/mozilla-mobile/fenix)

Fenix (internal codename) is the all-new Firefox for Android browser, based on [GeckoView](https://mozilla.github.io/geckoview/) and [Mozilla Android Components](https://mozac.org/).

<a href="https://play.google.com/store/apps/details?id=org.mozilla.firefox" target="_blank"><img src="https://play.google.com/intl/en_us/badges/images/generic/en-play-badge.png" alt="Get it on Google Play" height="90"/></a>

## Getting Involved

Please read the [Community Participation Guidelines](https://www.mozilla.org/en-US/about/governance/policies/participation/) and the [Bugzilla Etiquette guidelines](https://bugzilla.mozilla.org/page.cgi?id=etiquette.html) before filing an issue. This is our professional working environment as much as it is our bug tracker, and we want to keep our workspace clean and healthy.

* [Guide to Contributing](https://github.com/mozilla-mobile/shared-docs/blob/master/android/CONTRIBUTING.md) (**New contributors start here!**)

* Browse our [current Issues](https://github.com/mozilla-mobile/fenix/issues), or [file a security issue][sec issue].

* Matrix: [#fenix:mozilla.org channel](https://chat.mozilla.org/#/room/#fenix:mozilla.org) (**We're available Monday-Friday, GMT and PST working hours**). Related channels:
  * [#mobile-test-eng:mozilla.org channel](https://chat.mozilla.org/#/room/#mobile-test-eng:mozilla.org): for UI test automation
  * [#perf-android-frontend:mozilla.org channel](https://chat.mozilla.org/#/room/#perf-android-frontend:mozilla.org): for front-end (JVM) performance of Android apps
  * [#android-tips:mozilla.org channel](https://chat.mozilla.org/#/room/#android-tips:mozilla.org): for tips on Android development

* Check out the [project wiki](https://github.com/mozilla-mobile/fenix/wiki) for more information.
  * See [our guide on Writing Custom Lint Rules](https://github.com/mozilla-mobile/shared-docs/blob/master/android/writing_lint_rules.md).

* Localization happens on [Pontoon](https://pontoon.mozilla.org/projects/firefox-for-android/). Please get in touch with delphine (at) mozilla (dot) com directly for more information.

**Beginners!** - Watch out for [Issues with the "Good First Issue" label](https://github.com/mozilla-mobile/fenix/issues?q=is%3Aopen+is%3Aissue+label%3A%22good+first+issue%22). These are easy bugs that have been left for first timers to have a go, get involved and make a positive contribution to the project!


## I want to open a Pull Request!

We encourage you to participate in this open source project. We love Pull Requests, Bug Reports, ideas, (security) code reviews or any other kind of positive contribution.

Since we are a small team, however, **we do not have the bandwidth to review unsolicited PRs**. Please follow our [Pull Request guidelines](https://github.com/mozilla-mobile/shared-docs/blob/master/android/CONTRIBUTING_code.md#creating-a-pull-request), or **we may close the PR**.

To make it easier to review, we have these PR requirements:

* Every PR must have **exactly** one issue associated with it.
* Write a clear explanation of what the code is doing when opening the pull request, and optionally add comments to the PR.
* Make sure there are tests - or ask for help on how the code should be tested in the Issue!
* Keep PRs small and to the point. For extra code-health changes, either file a separate issue, or make it a separate PR that can be easily reviewed.
* Use micro-commits. This makes it easier and faster to review.
* Add a screenshot for UX changes (this is part of the PR checklist)

As a small team, we have to prioritize our work, and reviewing PRs takes time. We receive lots of PRs every day, so if you can keep your PRs small, it helps our small team review and merge code faster, minimizing stale code.


Keep in mind that the team is very overloaded, so PRs sometimes wait
for a *very* long time. However this is not for lack of interest, but
because we find ourselves in a constant need to prioritize
certain issues/PRs over others. If you think your issue/PR is very important,
try to popularize it by getting other users to comment and share their point of view.

## I want to file an issue!

Great! We encourage you to participate in this open source project. We love Pull Requests, Bug Reports, ideas, (security) code reviews or any other kind of positive contribution.

To make it easier to triage, we have these issue requirements:

* Please do your best to search for duplicate issues before filing a new issue so we can keep our issue board clean.
* Every issue should have **exactly** one bug/feature request described in it. Please do not file meta feedback list tickets as it is difficult to parse them and address their individual points.
* Feature Requests are better when they’re open-ended instead of demanding a specific solution -ie  “I want an easier way to do X” instead of “add Y”
* Issues are not the place to go off topic or debate. If you have questions, please join the [#fenix:mozilla.org channel](https://chat.mozilla.org/#/room/#fenix:mozilla.org).
* Please always remember our [Community Participation Guidelines](https://www.mozilla.org/en-US/about/governance/policies/participation/)
* Please do not tag specific team members to try to get your issue looked at faster. We have a triage process that will tag and label issues correctly in due time. If you think an issue is very severe, you can ask about it in Matrix.

Please keep in mind that even though a feature you have in mind may seem like a small ask, as a small team, we have to prioritize our planned work and every new feature adds complexity and maintenance and may take up design, research, marketing, product, and engineering time. We appreciate everyone’s passion but we will not be able to incorporate every feature request or even fix every bug. That being said, just because we haven't replied, doesn't mean we don't care about the issue, please be patient with our response times as we're very busy.

## Build Instructions

Pre-requisites:
* Android SDK
* To run command line tools, you'll need to configure Java: see [our how-to guide](https://github.com/mozilla-mobile/shared-docs/blob/master/android/configure_java.md).

1. Clone or Download the repository:

  ```shell
  git clone https://github.com/mozilla-mobile/fenix
  ```

2. **Import** the project into Android Studio **or** build on the command line:

  ```shell
  ./gradlew clean app:assembleDebug
  ```

  If this errors out, make sure that you have an `ANDROID_SDK_ROOT` environment
  variable pointing to the right path.

3. Make sure to select the correct build variant in Android Studio. See the next section.

4. Make sure to select "Default APK" under Installation Options inside Run/Debug configuration: see [this bug](https://bugzilla.mozilla.org/show_bug.cgi?id=1529082).

### Build Variants
For general development, we recommend the **debug** build variant. Here's an explanation of each variant:

- **debug**: the default for developers, similar to most other Android apps. It is debuggable, uses a Nightly GeckoView with debug symbols, adds tools like LeakCanary for troublingshooting, and does not strip unused code.
- **nightly**: what we ship to the Firefox Nightly channel, using GeckoView Nightly.
- **beta**: what we ship to the Firefox Beta channel, using GeckoView Beta. It is more stable than nightly.
- **release**: what we ship as Firefox for Android, using GeckoView Release. It is the most stable.

nightly, beta, and release are unsigned and `debuggable=false` by default. If
you want these variants to be:
- automatically signed, see [Automatically signing release builds](#automatically-sign-release-builds)
- `debuggable=true`, see [Building debuggable release variants](#building-debuggable-release-variants)

#### Performance Build Variants
For accurate performance measurements, read this section!

To analyze performance during **local development** build a production variant locally (this could either be the Nightly, beta or release).  Otherwise, you could also grab a pre-existing APK if you don't need to test some local changes. Then, use the Firefox profiler to profile what you need!

For more information on how to use the profiler or how to use the build, refer to this [how to measure performance with the build](https://wiki.mozilla.org/Performance/How_to_get_started_on_Fenix)

If you want to run **performance tests/benchmarks** in automation or locally use a production build since it is much closer in behavior compared to what users see in the wild.

Before you can install any release builds, **You will need to sign production build variants:** see [Automatically signing release builds](#automatically-sign-release-builds) for details.

##### Known disabled-by-default features
Some features are disabled by default when Fenix is built locally. This can be problematic at times for checking performance since you might want to know how your code behaves with those features.
The known features that are disabled by default are:
- Sentry
- Adjust
- Mozilla Location Services (also known as MLS)
- Firebase Push Services
- Telemetry (only disabled by default in debug builds)
- Nimbus

## Pre-push hooks
To reduce review turn-around time, we'd like all pushes to run tests locally. We'd
recommend you use our provided pre-push hook in `config/pre-push-recommended.sh`.
Using this hook will guarantee your hook gets updated as the repository changes.
This hook tries to run as much as possible without taking too much time.

Before you can run the hook, you'll need to configure Java properly because it relies on command line tools: see
[our how-to guide](https://github.com/mozilla-mobile/shared-docs/blob/master/android/configure_java.md).

To add it on Mac/Linux, run this command from the project root:
```sh
ln -s ../../config/pre-push-recommended.sh .git/hooks/pre-push
```
or for Windows run this command using the Command Prompt with administrative privileges:
```sh
mklink .git\hooks\pre-push ..\..\config\pre-push-recommended.sh
```
or using PowerShell:
```sh
New-Item -ItemType SymbolicLink -Path .git\hooks\pre-push -Value (Resolve-Path config\pre-push-recommended.sh)
```

To push without running the pre-push hook (e.g. doc updates):
```sh
git push <remote> --no-verify
```

Note: If while pushing you encounter this error "Could not initialize class org.codehaus.groovy.runtime.InvokerHelper" and are currently on Java14 then downgrading your Java version to Java13 or lower can resolve the issue

Steps to downgrade Java Version on Mac with Brew:
1. Install Homebrew (https://brew.sh/)
2. run ```brew update```
3. To uninstall your current java version, run ```sudo rm -fr /Library/Java/JavaVirtualMachines/<jdk-version>```
4. run ```brew tap homebrew/cask-versions```
5. run ```brew search java```
6. If you see java11, then run ```brew install java11```
7. Verify java-version by running ```java -version```

## local.properties helpers
You can speed up local development by setting a few helper flags available in `local.properties`. Some flags will make it easy to
work across multiple layers of the dependency stack - specifically, with android-components, geckoview or application-services.

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

### Setting raptor manifest flag

To set the raptor manifest flag in Nightly, Beta and Release variants, add the following to `<proj-root>/local.properties`:

```sh
raptorEnabled
```

### Auto-publication workflow for application-services and glean
If you're making changes to these projects and want to test them in Fenix, auto-publication workflow is the fastest, most reliable
way to do that.

In `local.properties`, specify a relative path to your local `glean` and/or `application-services` projects. E.g.:
- `autoPublish.glean.dir=../glean`
- `autoPublish.application-services.dir=../application-services`

Once these flags are set, your Fenix builds will include any local modifications present in these projects.

See a [demo of auto-publication workflow in action](https://www.youtube.com/watch?v=qZKlBzVvQGc).

In order to build successfully, you need to check out a commit in the dependency repository that has no breaking changes. The two best ways to do this are:
- Run the `<android-components>/tools/list_compatible_dependency_versions.py` script to output a compatible commit
- Check out the latest commit from main in this repository and the dependency repository. However, this may fail if there were breaking changes added recently to the dependency.

If you're trying to build fenix with a local ac AND a local GV, you'll have to use another method: see [this doc](https://github.com/mozilla-mobile/fenix/blob/main/docs/substituting-local-ac-and-gv.md).

### Using Nimbus servers during local development
If you're working with the Nimbus experiments platform, by default for local development Fenix configures Nimbus to not use a server.

If you wish to use a Nimbus server during local development, you can add a `https://` or `file://` endpoint to the `local.properties` file.

- `nimbus.remote-settings.url`

Testing experimental branches should be possible without a server.

### Using custom Glean servers during local development
If you wish to use a custom Glean server during local development, you can add a `https://` endpoint to the `local.properties` file.

- `glean.custom.server.url`

### GeckoView
For building with a local checkout of `mozilla-central` see [Substituting Local GeckoView](https://github.com/mozilla-mobile/firefox-android/blob/main/fenix/docs/substituting-local-gv.md)

## License


    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/

[sec issue]: https://bugzilla.mozilla.org/enter_bug.cgi?assigned_to=nobody%40mozilla.org&bug_ignored=0&bug_severity=normal&bug_status=NEW&cf_fx_iteration=---&cf_fx_points=---&component=Security%3A%20Android&contenttypemethod=list&contenttypeselection=text%2Fplain&defined_groups=1&flag_type-4=X&flag_type-607=X&flag_type-791=X&flag_type-800=X&flag_type-803=X&flag_type-936=X&flag_type-937=X&form_name=enter_bug&groups=mobile-core-security&maketemplate=Remember%20values%20as%20bookmarkable%20template&op_sys=Unspecified&priority=--&product=Fenix&rep_platform=Unspecified&target_milestone=---&version=unspecified
