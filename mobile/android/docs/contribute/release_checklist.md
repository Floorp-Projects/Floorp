---
layout: page
title: Release checklist
permalink: /contributing/release-checklist
---

These are instructions for preparing a release branch for Firefox Android and starting the next Nightly development cycle.

## [Release Management] Creating a new Beta release branch

**This part is 100% covered by the Release Management team. The dev team should not perform these steps.**

1. Create a branch name with the format `releases_v[beta_version]` off of the `main` branch (for example, `releases_v109`) through the GitHub UI.
`[beta_version]` should follow the Firefox Beta release number. See [Firefox Release Calendar](https://wiki.mozilla.org/Release_Management/Calendar).
2. In `main` [version.txt](https://github.com/mozilla-mobile/firefox-android/blob/main/version.txt): 
   - Update the version from `[previous_nightly_version].0a1` to `[nightly_version].0a1`. For example:
    ```diff
    diff --git a/version.txt b/version.txt
    --- a/version.txt
    +++ b/version.txt
    @@ -1 +1 @@
    -110.0a1
    +111.0a1
    ```
3. Create a commit named `Set version to [nightly_version].0a1` for this change. This change can either be directly committed to the `main` branch or a pull request can be created against it and then merged.
3. Notify the dev team that they can start the new Nightly development cycle per the steps given in the next section ⬇️
4. In the `releases_v[beta_version]` branch [version.txt](https://github.com/mozilla-mobile/firefox-android/blob/main/version.txt):
   - Update the version from `[beta_version].0a1` to `[beta_version].0b1`. For example:

    ```diff
    diff --git a/version.txt b/version.txt
    --- a/version.txt
    +++ b/version.txt
    @@ -1 +1 @@
    -109.0a1
    +109.0b1
    ```
5. Create a commit named `Set version to [beta_version].0b1` for this change. This change can either be directly committed to the `releases_v[beta_version]` branch or a pull request can be created against it and then merged.
6. In [Gecko.kt](https://github.com/mozilla-mobile/firefox-android/blob/main/android-components/plugins/dependencies/src/main/java/Gecko.kt):
   - Set the `channel` to `GeckoChannel.BETA`.

    ```diff
    diff --git a/android-components/plugins/dependencies/src/main/java/Gecko.kt b/android-components/plugins/dependencies/src/main/java/Gecko.kt
    --- a/android-components/plugins/dependencies/src/main/java/Gecko.kt
    +++ b/android-components/plugins/dependencies/src/main/java/Gecko.kt
    @@ -14,7 +14,7 @@ object Gecko {
         /**
          * GeckoView channel
          */
    -    val channel = GeckoChannel.NIGHTLY
    +    val channel = GeckoChannel.BETA
     }
    
     /**
    ```
7. Create a commit named `Switch to GeckoView Beta` for this change. This change can either be directly committed to the `releases_v[beta_version]` branch or a pull request can be created against it and then merged. Once landed, it is expected that this change will temporarily break builds on the branch. The next step will fix them.
8. The Github Action will automatically bump the GeckoView version once the new Beta build is created and uploaded to Maven, as shown in [#324](https://github.com/mozilla-mobile/firefox-android/pull/324) for example.
9. Once both of the above commits have landed, create a new `Firefox Android (Android-Components, Femix, Focus)` release in [Ship-It](https://shipit.mozilla-releng.net/) and continue with the release checklist per normal practice.

## [Dev team] Starting the next Nightly development cycle

**Please handle this part once Release Management gives you the go.**

0. Wait for greenlight coming from Release Management.
1. Create a local development branch off of the `main` branch.
2. Create a commit named `Update changelog for v[nightly_version] release` that will update the following in [changelog.md](https://github.com/mozilla-mobile/firefox-android/blob/main/docs/changelog.md):
  - Add a new `v[nightly_version] (In Development)` section for the next Nightly version with the next commit and milestone numbers.
  - Update the `v[beta_version]` section, change `blob` in the links to `v[beta_version]` and specifying the correct commit ranges.

    ```diff
    diff --git a/docs/changelog.md b/docs/changelog.md
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

3. Create a pull request with this commit and land.
  - If this is landed after the scheduled [cron task](https://github.com/mozilla-mobile/firefox-android/blob/main/.cron.yml#L13) that will build and bump AC automatically, trigger a manual AC build through the [hook](https://firefox-ci-tc.services.mozilla.com/hooks/project-releng/cron-task-mozilla-mobile-android-components%2Fnightly). At time of writing, the morning cron task is schedule to run at 04:00 UTC (11:00AM EST) and 16:00 UTC (11:00PM EST).
4. After an hour, follow up by checking if a new `[nightly_version]` AC build is available in [nightly.maven/components](https://nightly.maven.mozilla.org/?prefix=maven2/org/mozilla/components/).

See [https://github.com/mozilla-mobile/android-components/pull/12956](https://github.com/mozilla-mobile/android-components/pull/12956) for an example.

## After the release

1. Verify that the links in the [changelog.md](https://github.com/mozilla-mobile/firefox-android/blob/main/docs/changelog.md) and new release are working.
2. Verify that Fenix is receiving the latest `[nightly_version]` AC version in `main`.
3. Send release message to the [Matrix channel](https://chat.mozilla.org/#/room/#android-components:mozilla.org):
```
**Android Components 0.17 Release (:ocean:)**
Release notes: https://mozac.org/changelog/
Milestone: https://github.com/mozilla-mobile/firefox-android/milestone/15?closed=1
```
