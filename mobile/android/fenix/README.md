# Firefox Preview

[![Task Status](https://github.taskcluster.net/v1/repository/mozilla-mobile/fenix/master/badge.svg)](https://github.taskcluster.net/v1/repository/mozilla-mobile/fenix/master/latest)
[![codecov](https://codecov.io/gh/mozilla-mobile/fenix/branch/master/graph/badge.svg)](https://codecov.io/gh/mozilla-mobile/fenix)

Firefox Preview (internal code name: "Fenix") is an all-new browser for Android, based on [GeckoView](https://mozilla.github.io/geckoview/) and [Mozilla Android Components](https://mozac.org/).

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

## Getting Involved

Before you attempt to make a contribution please read the [Community Participation Guidelines](https://www.mozilla.org/en-US/about/governance/policies/participation/).

* [Guide to Contributing](https://github.com/mozilla-mobile/shared-docs/blob/master/android/CONTRIBUTING.md) (**New contributors start here!**)

* Browse our [current Issues](https://github.com/mozilla-mobile/fenix/issues), or [file a security issue][sec issue].

* Slack: [#fenix on mozilla-android.slack.com](mozilla-android.slack.com) | [Join here](https://join.slack.com/t/mozilla-android/shared_invite/enQtNzk3NDczMzc0ODMzLWNjYTQ5MjA3ZDI0NzNhOTBjMWUyMmNmZDcwM2ZhY2YxZjdjMWIwZWY1YmQ3NjJmMDNkODYxMmRmNTdjNGJmMTU)
(**We're available Monday-Friday, GMT and PST working hours**).

* Check out the [project wiki](https://github.com/mozilla-mobile/fenix/wiki) for more information.

* Localization happens on [Pontoon](https://pontoon.mozilla.org/projects/android-l10n/). Please get in touch with delphine (at) mozilla (dot) com directly for more information.

**Beginners!** - Watch out for [Issues with the "Good First Issue" label](https://github.com/mozilla-mobile/fenix/issues?q=is%3Aopen+is%3Aissue+label%3A%22good+first+issue%22). These are easy bugs that have been left for first timers to have a go, get involved and make a positive contribution to the project!

## Build Instructions

Note: Both Android SDK and NDK are required.

1. Clone or Download the repository:

  ```shell
  git clone https://github.com/mozilla-mobile/fenix
  ```

2. **Import** the project into Android Studio **or** build on the command line:

  ```shell
  ./gradlew clean app:assembleGeckoBetaDebug
  ```
  
  Use app:assembleGeckoNightlyDebug to build with the Gecko Nightly version instead.

3. Make sure to select the correct build variant in Android Studio. See the next section.

### Guide to Build Variants
We have a lot of build variants. Each variant is composed of two flavors. One flavor is the version of Gecko to use and the other describes 
which app id and settings to use. Here is a description of what each means:

- **geckoBeta** (recommended) uses the Beta variant of the Gecko rendering engine, which corresponds to the next version of Gecko which will go to production
- **geckoNightly** uses the Nightly variant of the Gecko rendering engine, which is the version which will arrive after beta and is less stable

<br />
<br />

- **debug** uses debug symbols and debug signing, adds tools like LeakCanary for troubleshooting, and does not strip unused or wasteful code
- **fenixNightly** is a release build with nightly signing which uses the org.mozilla.fenix.nightly app id for nightly releases to Google Play
- **fenixBeta** is a release build with beta signing which uses the org.mozilla.fenix.beta app id for beta releases to Google Play
- **fenixProduction** is a release build with release signing which uses the org.mozilla.fenix app id for production releases to Google Play
- **fennecProduction** is an experimental build with release signing which uses the org.mozilla.firefox app id and supports upgrading the older Firefox. **WARNING** Pre-production versions of this may result in data loss.
- **forPerformanceTest** is a release build with the debuggable flag set and test activities enabled for running Raptor performance tests

## Pre-push hooks
To reduce review turn-around time, we'd like all pushes to run tests locally. We'd
recommend you use our provided pre-push hook in `config/pre-push-recommended.sh`.
Using this hook will guarantee your hook gets updated as the repository changes.
This hook tries to run as much as possible without taking too much time.

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

## local.properties helpers
There are multiple helper flags available via `local.properties` that will help speed up local development workflow
when working across multiple layers of the dependency stack - specifically, with android-components, geckoview or application-services.

### android-components auto-publication workflow
Specify a relative path to your local `android-components` checkout via `autoPublish.android-components.dir`.

If enabled, during a Fenix build android-components will be compiled and locally published if it has been modified,
and published versions of android-components modules will be automatically used instead of whatever is declared in Dependencies.kt.

### application-services composite builds
Specify a relative path to your local `application-services` checkout via `substitutions.application-services.dir`.

If enabled, a multi-project gradle build will be configured, and any application-services dependency will be substituted
for the local version. Any changes to `application-services` will be automatically included in Fenix builds.

### GeckoView
Specify a relative path to your local `mozilla-central` checkout via `dependencySubstitutions.geckoviewTopsrcdir`,
and optional a path to m-c object directory via `dependencySubstitutions.geckoviewTopobjdir`.

If these are configured, local builds of GeckoView will be used instead of what's configured in Dependencies.kt.
For more details, see https://mozilla.github.io/geckoview/contributor/geckoview-quick-start#include-geckoview-as-a-dependency

## License


    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/

[sec issue]: https://bugzilla.mozilla.org/enter_bug.cgi?assigned_to=nobody%40mozilla.org&bug_ignored=0&bug_severity=normal&bug_status=NEW&cf_fx_iteration=---&cf_fx_points=---&component=Security%3A%20Android&contenttypemethod=list&contenttypeselection=text%2Fplain&defined_groups=1&flag_type-4=X&flag_type-607=X&flag_type-791=X&flag_type-800=X&flag_type-803=X&flag_type-936=X&flag_type-937=X&form_name=enter_bug&groups=mobile-core-security&maketemplate=Remember%20values%20as%20bookmarkable%20template&op_sys=Unspecified&priority=--&product=Fenix&rep_platform=Unspecified&target_milestone=---&version=unspecified
