---
layout: page
title: Release checklist
permalink: /contributing/release-checklist
---

These are instructions for preparing a release branch for Firefox Android and starting the next Nightly development cycle.

## [Release Engineering / Release Management] Beta release branch creation & updates

**This part is covered by the Release Engineering and Release Management teams. The dev team should not perform these steps.**

1. Release Management emails the release-drivers list to begin merge day activities. For example, `Please merge mozilla-central to mozilla-beta, bump central to 123.` A message will also be sent to the #releaseduty Matrix channel.
2. The Release Engineering engineer on duty (“relduty”) triggers the Taskcluster merge day hooks. The hooks automate the following tasks:
    - Migrating tip of mozilla-central to mozilla-beta
    - Updating version.txt on mozilla-central for the new Nightly version
    - Updating version.txt on mozilla-beta to change the version string from `[beta_version].0a1` to `[beta_version].0b1`
3. When the merge day tasks are completed, relduty will respond to the release-drivers thread and #releaseduty channel that merges are completed. This also serves as notification to the dev team that they can start the new Nightly development cycle per the steps given in the next section ⬇️
4. In [ApplicationServices.kt](https://hg.mozilla.org/mozilla-central/file/default/mobile/android/android-components/plugins/dependencies/src/main/java/ApplicationServices.kt):
    - Set `VERSION` to `[major-version].0`
    - Set `CHANNEL` to `ApplicationServicesChannel.RELEASE`

    ```diff
    --- a/mobile/android/android-components/plugins/dependencies/src/main/java/ApplicationServices.kt
    +++ b/mobile/android/android-components/plugins/dependencies/src/main/java/ApplicationServices.kt
    -val VERSION = "121.20231118050255"
    -val CHANNEL = ApplicationServicesChannel.NIGHTLY
    +val VERSION = "121.0"
    +val CHANNEL = ApplicationServicesChannel.RELEASE
    ```
    - Create a commit named `Switch to Application Services Release`. (Note: application-services releases directly after the nightly cycle, there's no beta cycle).
5. Once all of the above commits have landed, create a new `Firefox Android (Android-Components, Fenix, Focus)` release in [Ship-It](https://shipit.mozilla-releng.net/) and continue with the release checklist per normal practice.

## [Dev team] Starting the next Nightly development cycle

**Please handle this part once Release Management gives you the go.**

Now that we made the Beta cut, we can remove all the unused strings marked moz:removedIn <= `[release_version subtract 1]`. `[release_version]` should follow the Firefox Release version. See [Firefox Release Calendar](https://wiki.mozilla.org/Release_Management/Calendar) for the current Release version. We will also want to bump the Android Component's [changelog.md](https://hg.mozilla.org/mozilla-central/file/default/mobile/android/android-components/docs/changelog.md) with the new Nightly development section.

0. Wait for greenlight coming from Release Engineering (see #3 above).
1. File a Bugzilla issue named "Start the Nightly `[nightly_version]` development cycle".
2. Search and remove all strings marked `moz:removedIn="[release_version subtract 1]"` across Fenix, Focus and Android Components. Please avoid removing strings in the localized `strings.xml` and limit changes only to `values/strings.xml`.
3. Add the next Nightly in development section in the [changelog.md](https://hg.mozilla.org/mozilla-central/file/default/mobile/android/android-components/docs/changelog.md).:
  - Add a new `[nightly_version].0 (In Development)` section for the next Nightly version with the next commit and milestone numbers.
  - Update the `[beta_version].0` section, update the links to `release_v[beta_version]` and specifying the correct commit ranges. This should equate to changing `/blob/main/` to `/blob/releases_v[beta_version]/`.

      ```diff
      diff --git a/docs/changelog.md b/docs/changelog.md
      index 9e95d0e2adc..d901ed38cdd 100644
      --- a/docs/changelog.md
      +++ b/docs/changelog.md
      @@ -4,12 +4,18 @@ title: Changelog
      permalink: /changelog/
      ---

      -# 114.0 (In Development)
      -* [Commits](https://github.com/mozilla-mobile/firefox-android/compare/releases_v113..main)
      +# 115.0 (In Development)
      +* [Commits](https://github.com/mozilla-mobile/firefox-android/compare/releases_v114..main)
      * [Dependencies](https://github.com/mozilla-mobile/firefox-android/blob/main/android-components/plugins/dependencies/src/main/java/DependenciesPlugin.kt)
      * [Gecko](https://github.com/mozilla-mobile/firefox-android/blob/main/android-components/plugins/dependencies/src/main/java/Gecko.kt)
      * [Configuration](https://github.com/mozilla-mobile/firefox-android/blob/main/android-components/.config.yml)

      +# 114.0
      +* [Commits](https://github.com/mozilla-mobile/firefox-android/compare/releases_v113..releases_v114)
      +* [Dependencies](https://github.com/mozilla-mobile/firefox-android/blob/releases_v114/android-components/plugins/dependencies/src/main/java/DependenciesPlugin.kt)
      +* [Gecko](https://github.com/mozilla-mobile/firefox-android/blob/releases_v114/android-components/plugins/dependencies/src/main/java/Gecko.kt)
      +* [Configuration](https://github.com/mozilla-mobile/firefox-android/blob/releases_v114/android-components/.config.yml)
      +
      * * **browser-state**
        * 🌟 Added `DownloadState`.`openInApp` to indicate whether or not the file associated with the download should be opened in a third party app after downloaded successfully, for more information see [bug 1829371](https://bugzilla.mozilla.org/show_bug.cgi?id=1829371) and [bug 1829372](https://bugzilla.mozilla.org/show_bug.cgi?id=1829372).
      ```

4. Submit a patch for review and land.

### [Dev Team] Renew telemetry

After the Beta cut, another task is to remove all soon to expire telemetry probes. What we're looking for is to create a list of telemetry that will expire in `[nightly_version add 1]`. See [Firefox Release Calendar](https://whattrainisitnow.com/calendar/) for the current Release version. There is a script that will help with finding these soon to expire telemetry.

1. Use the helper in the mobile/android/fenix/tools folder `python3 data_renewal_generate.py [nightly_version add 1]` to detect and generate files that will help create the following files:
    - `[nightly_version add 1]`_expiry_list.csv
2. File an issue for removing expired telemetry to address the expired metrics. See [Bug 1881336](https://bugzilla.mozilla.org/show_bug.cgi?id=1881336) for an example.
3. Remove the expired metrics.  See [example](https://github.com/mozilla-mobile/firefox-android/pull/5700).

### [Dev Team] Add SERP Telemetry json dump

After the beta cut, another task is to add SERP telemetry json to the [search-telemetry-v2.json](https://hg.mozilla.org/mozilla-central/file/default/mobile/android/android-components/components/feature/search/src/main/assets/search/search_telemetry_v2.json)
The dump is to be fetched from the desktop telemetry dump located at [desktop-search-telemetry-v2.json](https://searchfox.org/mozilla-central/source/services/settings/dumps/main/search-telemetry-v2.json)

### Ask for Help

- Issues related to releases `#releaseduty` on Element
- Topics about CI (and the way we use Taskcluster) `#Firefox CI` on Element
- Breakage in PRs due to Gradle issues or GV upgrade problems `#mobile-android-team` on Slack
