---
layout: page
title: Release checklist
permalink: /contributing/release-checklist
---

This is instructions for preparing a release branch for Android Components Beta release and starting the next development cycle.

## Creating a new Beta release branch

1. Create a branch name with the format `releases/[beta_version].0` off of the `main` branch (for example, `releases/98.0`) through the GitHub UI.
`[beta_version]` should follow the Firefox Beta release number. See [Firefox Release Calendar](https://wiki.mozilla.org/Release_Management/Calendar).
2. In [Gecko.kt](https://github.com/mozilla-mobile/android-components/blob/main/buildSrc/src/main/java/Gecko.kt):
   - Set the `version` to the first build of GeckoView `[beta_version]` that is available in [maven/geckoview-beta](https://maven.mozilla.org/?prefix=maven2/org/mozilla/geckoview/geckoview-beta/).
   - Set the `channel` to `val channel = GeckoChannel.BETA`.

    ```diff
    diff --git a/buildSrc/src/main/java/Gecko.kt b/buildSrc/src/main/java/Gecko.kt
    index 991dd702bb5..3b110e803c7 100644
    --- a/buildSrc/src/main/java/Gecko.kt
    +++ b/buildSrc/src/main/java/Gecko.kt
    @@ -9,12 +9,12 @@ object Gecko {
      /**
       * GeckoView Version.
       */
    -    const val version = "98.0.20220207065816"
    +    const val version = "98.0.20220207151916"

       /**
        * GeckoView channel
        */
    -    val channel = GeckoChannel.NIGHTLY
    +    val channel = GeckoChannel.BETA
        }
    ```

3. Create a commit and pull request named `Update to first GeckoView [beta_version] Beta` for this change. The pull request should be committed into the `releases/[beta_version].0` branch.
4. After this pull request has landed, [Draft a new release](https://github.com/mozilla-mobile/android-components/releases/new):
    - Release title: `Android-Components [beta_version].0.0`
    - Create a new tag and select it in "Choose a tag": `v[beta_version.0.0]`
    - Target: `releases/[beta_version].0`
    - Description: Follow this template and bump the release note URL, specify the correct commit range and milestone.

    ```
    * [Release notes](https://mozac.org/changelog/#9800)
    * [Commits](https://github.com/mozilla-mobile/android-components/compare/v97.0.0...v98.0.0)
    * [Milestone](https://github.com/mozilla-mobile/android-components/milestone/145?closed=1)
    ```

    - Push the "Publish release.

    <img width="1385" alt="Screen Shot 2022-02-08 at 9 55 58 AM" src="https://user-images.githubusercontent.com/1190888/153047976-cffdf1c1-e3c1-45eb-83ed-cedd021a68b4.png">

5. After 30-60 minutes, follow up to see if a `[beta_version].0.0` build is available in one of the AC components on [maven/components](https://maven.mozilla.org/?prefix=maven2/org/mozilla/components/).

See [https://github.com/mozilla-mobile/android-components/pull/11520](https://github.com/mozilla-mobile/android-components/pull/11520) for an example.

## Starting the next Nightly development cycle

1. Create a local development branch off of the `main` branch.
2. Create a commit named `Start [nightly_version].0.0 development cycle` In [.buildconfig.yml](https://github.com/mozilla-mobile/android-components/blob/main/.buildconfig.yml) and [version.txt](https://github.com/mozilla-mobile/android-components/blob/main/version.txt), bump the current `componentsVersion` up by 1 and ensure this change is also synced with `version.txt`. `[nightly_version]` should follow the Firefox Nightly release number. See [Firefox Release Calendar](https://wiki.mozilla.org/Release_Management/Calendar).

    ```diff
    diff --git a/.buildconfig.yml b/.buildconfig.yml
    index 62e0d8729b9..4f3f9cb5f94 100644
    --- a/.buildconfig.yml
    +++ b/.buildconfig.yml
    @@ -1,6 +1,6 @@
    # Please keep this version in sync with version.txt
    # version.txt should be the new source of truth for version numbers
    -componentsVersion: 98.0.0
    +componentsVersion: 99.0.0
    # Please add a treeherder group in taskcluster/ci/config.yml if you add a new project here.
    projects:
      compose-awesomebar:
    diff --git a/version.txt b/version.txt
    index be9ad8dac55..511c658edc3 100644
    --- a/version.txt
    +++ b/version.txt
    @@ -1 +1 @@
    -98.0.0
    +99.0.0
    ```

3. Create a new [Milestone](https://github.com/mozilla-mobile/android-components/milestones) with the following format `[nightly_version].0.0 <insert an emoji>` (for example, `99.0.0 🛎`). Close the existing milestone and bump all the remaining opened issues to the next milestone or simply remove the tagged milestone depending on whhat is appropriate.
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

5. Create a commit named `Update to GeckoView [nightly_version].0` that bumps the GeckoView `version` in [Gecko.kt](https://github.com/mozilla-mobile/android-components/blob/main/buildSrc/src/main/java/Gecko.kt) to the latest `[nightly_version]` build when available in [maven/geckoview-nightly](https://maven.mozilla.org/?prefix=maven2/org/mozilla/geckoview/geckoview-nightly/).

    ```diff
    diff --git a/buildSrc/src/main/java/Gecko.kt b/buildSrc/src/main/java/Gecko.kt
    index 991dd702bb5..6f6c4471a26 100644
    --- a/buildSrc/src/main/java/Gecko.kt
    +++ b/buildSrc/src/main/java/Gecko.kt
    @@ -9,7 +9,7 @@ object Gecko {
        /**
          * GeckoView Version.
          */
    -    const val version = "98.0.20220207065816"
    +    const val version = "99.0.20220208070047"

        /**
          * GeckoView channel
    ```

6. Create a pull request with these 3 commits and land.
  - If this is landed after the scheduled [cron task](https://github.com/mozilla-mobile/android-components/blob/main/.cron.yml#L13) that will build and bump AC automatically, trigger a manual AC build through the [hook](https://firefox-ci-tc.services.mozilla.com/hooks/project-releng/cron-task-mozilla-mobile-android-components%2Fnightly). At time of writing, the morning cron task is schedule to run at 14:30 UTC (9:30AM EST).
  - When the manual AC build is complete, trigger the [hook](https://firefox-ci-tc.services.mozilla.com/hooks/project-releng/cron-task-mozilla-mobile-fenix%2Fbump-android-components) to bump AC in Fenix.
6. After an hour, follow up by checking if a new `[nightly_version]` AC build is available in [nightly.maven/components](https://nightly.maven.mozilla.org/?prefix=maven2/org/mozilla/components/). Fenix will automatically receive the Nightlly AC bump.

See [https://github.com/mozilla-mobile/android-components/pull/11519](https://github.com/mozilla-mobile/android-components/pull/11519) for an example.

## After the release

1. Verify that the links in the [changelog.md](https://github.com/mozilla-mobile/android-components/blob/main/docs/changelog.md) and new release are working.
2. Verify that Fenix is receiving the latest `[nightly_version]` AC version in `main`.
3. Send release message to the [Matrix channel](https://chat.mozilla.org/#/room/#android-components:mozilla.org):
```
**Android Components 0.17 Release (:ocean:)**
Release notes: https://mozilla-mobile.github.io/android-components/changelog/
Milestone: https://github.com/mozilla-mobile/android-components/milestone/15?closed=1
```
