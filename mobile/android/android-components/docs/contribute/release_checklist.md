---
layout: page
title: Release checklist
permalink: /contributing/release-checklist
---

These are instructions for preparing a release branch for Android Components and starting the next Nightly development cycle.

## [Release Management] Creating a new Beta release branch

**This part is 100% covered by the Release Management team. The dev team should not perform these steps.**

1. Create a branch name with the format `releases/[beta_version].0` off of the `main` branch (for example, `releases/98.0`) through the GitHub UI.
`[beta_version]` should follow the Firefox Beta release number. See [Firefox Release Calendar](https://wiki.mozilla.org/Release_Management/Calendar).
2. Notify the dev team that they can start the new Nightly development cycle per the steps given in the next section ⬇️
3. In [Gecko.kt](https://github.com/mozilla-mobile/android-components/blob/main/buildSrc/src/main/java/Gecko.kt):
   - Set the `channel` to `val channel = GeckoChannel.BETA`.

    ```diff
    diff --git a/buildSrc/src/main/java/Gecko.kt b/buildSrc/src/main/java/Gecko.kt
    index 80b4113cae..bea6243f69 100644
    --- a/buildSrc/src/main/java/Gecko.kt
    +++ b/buildSrc/src/main/java/Gecko.kt
    @@ -14,7 +14,7 @@ object Gecko {
        /**
          * GeckoView channel
          */
    -    val channel = GeckoChannel.NIGHTLY
    +    val channel = GeckoChannel.BETA
    }

    /**
    ```
4. Create a commit named `Switch to GeckoView Beta` for this change. This change can either be directly committed to the newly-created `releases/[beta_version].0` branch or a pull request can be created against it and then merged. Once landed, it is expected that this will create busted builds. The next step should green it up.
5. The Github Action will automatically bump the GeckoView version once the new Beta build is created and uploaded to Maven, as shown in [#12547](https://github.com/mozilla-mobile/android-components/pull/12547) for example.
6. Once both of the above commits have landed, create a new AC release in [Ship-It](https://shipit.mozilla-releng.net/) per normal practice.
7. When the release automation completes, a new `v[beta_version].0.0` release tag will be created in Github.
   - Following the template below, update the description with the release notes URL, commit range, and milestone like shown in [v104.0.0](https://github.com/mozilla-mobile/android-components/releases/tag/v104.0.0) and click `Update release` when ready.

    ```
    * [Release notes](https://mozac.org/changelog/#9800)
    * [Commits](https://github.com/mozilla-mobile/android-components/compare/v97.0.0...v98.0.0)
    * [Milestone](https://github.com/mozilla-mobile/android-components/milestone/145?closed=1)
    ```

## [Dev team] Starting the next Nightly development cycle

**Please handle this part once Release Management gives you the go.**

0. Wait for greenlight coming from Release Management.
1. Create a local development branch off of the `main` branch.
2. Create a commit named `Start [nightly_version].0.0 development cycle`. In [version.txt](https://github.com/mozilla-mobile/android-components/blob/main/version.txt), bump the major number by 1. `[nightly_version]` should follow the Firefox Nightly release number. See [Firefox Release Calendar](https://wiki.mozilla.org/Release_Management/Calendar).

    ```diff
    diff --git a/version.txt b/version.txt
    index be9ad8dac55..511c658edc3 100644
    --- a/version.txt
    +++ b/version.txt
    @@ -1 +1 @@
    -98.0.0
    +99.0.0
    ```

3. Create a new [Github Milestone](https://github.com/mozilla-mobile/android-components/milestones) with the following format `[nightly_version].0.0 <insert an emoji>` (for example, `99.0.0 🛎`). Close the existing milestone and bump all the remaining opened issues to the next milestone or simply remove the tagged milestone depending on what is appropriate.
4. Create a commit named `Update changelog for [nightly_version].0.0 release` that will update the following in [changelog.md](https://github.com/mozilla-mobile/android-components/blob/main/docs/changelog.md):
  - Add a new `[nightly_version].0.0 (In Development)` section for the next Nightly version of AC with the next commit and milestone numbers.
  - Update the `[beta_version].0.0` section, change `blob` in the links to `v[beta_version].0.0` and specifying the correct commit ranges.

    ```diff
    diff --git a/docs/changelog.md b/docs/changelog.md
    index 895fa9d69f0..f62d9ea6efe 100644
    --- a/docs/changelog.md
    +++ b/docs/changelog.md
    @@ -4,20 +4,26 @@ title: Changelog
    permalink: /changelog/
    ---

    -# 98.0.0 (In Development)
    -* [Commits](https://github.com/mozilla-mobile/android-components/compare/v97.0.0...main)
    -* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/145?closed=1)
    +# 99.0.0 (In Development)
    +* [Commits](https://github.com/mozilla-mobile/android-components/compare/v98.0.0...main)
    +* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/146?closed=1)
    * [Dependencies](https://github.com/mozilla-mobile/android-components/blob/main/buildSrc/src/main/java/Dependencies.kt)
    * [Gecko](https://github.com/mozilla-mobile/android-components/blob/main/buildSrc/src/main/java/Gecko.kt)
    * [Configuration](https://github.com/mozilla-mobile/android-components/blob/main/.config.yml)

    +# 98.0.0
    +* [Commits](https://github.com/mozilla-mobile/android-components/compare/v97.0.0...v98.0.0)
    +* [Milestone](https://github.com/mozilla-mobile/android-components/milestone/145?closed=1)
    +* [Dependencies](https://github.com/mozilla-mobile/android-components/v98.0.0/main/buildSrc/src/main/java/Dependencies.kt)
    +* [Gecko](https://github.com/mozilla-mobile/android-components/v98.0.0/main/buildSrc/src/main/java/Gecko.kt)
    +* [Configuration](https://github.com/mozilla-mobile/android-components/v98.0.0/main/.config.yml)
    +
    ```

5. Create a pull request with these 2 commits and land.
  - If this is landed after the scheduled [cron task](https://github.com/mozilla-mobile/android-components/blob/main/.cron.yml#L13) that will build and bump AC automatically, trigger a manual AC build through the [hook](https://firefox-ci-tc.services.mozilla.com/hooks/project-releng/cron-task-mozilla-mobile-android-components%2Fnightly). At time of writing, the morning cron task is schedule to run at 14:30 UTC (9:30AM EST).
  - When the manual AC build is complete, trigger the [hook](https://firefox-ci-tc.services.mozilla.com/hooks/project-releng/cron-task-mozilla-mobile-fenix%2Fbump-android-components) to bump AC in Fenix.
6. After an hour, follow up by checking if a new `[nightly_version]` AC build is available in [nightly.maven/components](https://nightly.maven.mozilla.org/?prefix=maven2/org/mozilla/components/). Fenix and Focus will automatically receive the Nightly AC bump.

See [https://github.com/mozilla-mobile/android-components/pull/12956](https://github.com/mozilla-mobile/android-components/pull/12956) for an example.

## After the release

1. Verify that the links in the [changelog.md](https://github.com/mozilla-mobile/android-components/blob/main/docs/changelog.md) and new release are working.
2. Verify that Fenix is receiving the latest `[nightly_version]` AC version in `main`.
3. Send release message to the [Matrix channel](https://chat.mozilla.org/#/room/#android-components:mozilla.org):
```
**Android Components 0.17 Release (:ocean:)**
Release notes: https://mozilla-mobile.github.io/android-components/changelog/
Milestone: https://github.com/mozilla-mobile/android-components/milestone/15?closed=1
```
