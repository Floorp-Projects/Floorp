# Firefox Preview

[![Task Status](https://github.taskcluster.net/v1/repository/mozilla-mobile/fenix/master/badge.svg)](https://github.taskcluster.net/v1/repository/mozilla-mobile/fenix/master/latest)
[![codecov](https://codecov.io/gh/mozilla-mobile/fenix/branch/master/graph/badge.svg)](https://codecov.io/gh/mozilla-mobile/fenix)

Firefox Preview (internal code name: "Fenix") is an all-new browser for Android, based on [GeckoView](https://mozilla.github.io/geckoview/) and [Mozilla Android Components](https://mozac.org/).

** Note: The team is currently experiencing heavy triage and review load, so when triaging issues, we will mainly be looking to identify [S1 (high severity)](https://github.com/mozilla-mobile/fenix/labels/S1) issues. See our triage process [here](https://github.com/mozilla-mobile/fenix/wiki/Triage-Process). Please be patient if you don't hear back from us immediately on your issue! **

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

* Matrix: [#fenix:mozilla.org channel](https://chat.mozilla.org/#/room/#fenix:mozilla.org) (**We're available Monday-Friday, GMT and PST working hours**). Related channels:
  * [#mobile-test-eng:mozilla.org channel](https://chat.mozilla.org/#/room/#mobile-test-eng:mozilla.org): for UI test automation
  * [#perf-android-frontend:mozilla.org channel](https://chat.mozilla.org/#/room/#perf-android-frontend:mozilla.org): for front-end (JVM) performance of Android apps
  * [#android-tips:mozilla.org channel](https://chat.mozilla.org/#/room/#android-tips:mozilla.org): for tips on Android development

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
- **forPerformanceTest**: see "Performance Build Variants" below.

#### Performance Build Variants
For accurate performance measurements, read this section!

If you want to analyze performance during **local development** (note: there is a non-trivial performance impact - see caveats):
- Recommendation: use a `forPerformanceTest` variant with local Leanplum, Adjust, & Sentry API tokens: contact the front-end perf group for access to them
- Rationale: `forPerformanceTest` is a release variant with `debuggable` set to true. There are numerous performance-impacting differences between debug and release variants so we need a release variant. To profile, we also need debuggable, which is disabled on other release variants. If API tokens are not provided, the SDKs may change their behavior in non-trivial ways.
- Caveats:
  - debuggable has a non-trivial & variable impact on performance but is needed to take profiles.
  - Random experiment opt-in & feature flags may impact performance (see [perf-frontend-issues#45](https://github.com/mozilla-mobile/perf-frontend-issues/issues/45) for mitigation).
  - This is slower to build than debug builds because it does additional tasks (e.g. minification) similar to other release builds

Nightly `forPerformanceTest` variants with API tokens already added [are also available from Taskcluster](https://firefox-ci-tc.services.mozilla.com/tasks/index/project.mobile.fenix.v2.performance-test/).

If you want to run **performance tests/benchmarks** in automation or locally:
- Recommendation: production builds. If debuggable is required, use recommendation above but note the caveat above. If your needs are not met, please contact the front-end perf group to identify a new solution.
- Rationale: like the rationale above, we need release variants so the choice is based on the debuggable flag.

For additional context on these recommendations, see [the perf build variant analysis](https://docs.google.com/document/d/1aW-m0HYncTDDiRz_2x6EjcYkjBpL9SHhhYix13Vil30/edit#).

You will **need to sign `forPerformanceTest` variants.** For local development, our recommendation is to add the following configuration to `app/build.gradle`:

```groovy
android { // this line already exists
    // ...

    buildTypes { // this line already exists
        // ...

        forPerformanceTest releaseTemplate >> { // this line already exists.
            // ...

            signingConfig signingConfigs.debug
        }
    }
}
```

This recommendation will let you use AS just like you do with debug builds but **please do not check in these changes.**

See [perf-frontend-issues#44](https://github.com/mozilla-mobile/perf-frontend-issues/issues/44) for efforts to make performance signing easier.

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

### Auto-publication workflow for android-components and application-services
If you're making changes to these projects and want to test them in Fenix, auto-publication workflow is the fastest, most reliable
way to do that.

In `local.properties`, specify a relative path to your local `android-components` and/or `application-services` checkouts. E.g.:
- `autoPublish.android-components.dir=../android-components`
- `autoPublish.application-services.dir=../application-services`

Once these flags are set, your Fenix builds will include any local modifications present in these projects.

See a [demo of auto-publication workflow in action](https://www.youtube.com/watch?v=qZKlBzVvQGc).

### GeckoView
Specify a relative path to your local `mozilla-central` checkout via `dependencySubstitutions.geckoviewTopsrcdir`,
and optional a path to m-c object directory via `dependencySubstitutions.geckoviewTopobjdir`.

If these are configured, local builds of GeckoView will be used instead of what's configured in Dependencies.kt.
For more details, see https://firefox-source-docs.mozilla.org/mobile/android/geckoview/contributor/geckoview-quick-start.html#include-geckoview-as-a-dependency

## License


    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/

[sec issue]: https://bugzilla.mozilla.org/enter_bug.cgi?assigned_to=nobody%40mozilla.org&bug_ignored=0&bug_severity=normal&bug_status=NEW&cf_fx_iteration=---&cf_fx_points=---&component=Security%3A%20Android&contenttypemethod=list&contenttypeselection=text%2Fplain&defined_groups=1&flag_type-4=X&flag_type-607=X&flag_type-791=X&flag_type-800=X&flag_type-803=X&flag_type-936=X&flag_type-937=X&form_name=enter_bug&groups=mobile-core-security&maketemplate=Remember%20values%20as%20bookmarkable%20template&op_sys=Unspecified&priority=--&product=Fenix&rep_platform=Unspecified&target_milestone=---&version=unspecified
