---
layout: page
title: Synchronizing the branching and versioning of Android Components with the Mozilla release trains
permalink: /rfc/0007-synchronized-releases
---

* Start date: 2021-03-22
* RFC PR: [#9973](https://github.com/mozilla-mobile/android-components/pull/9973)

## Summary

Synchronizing the branching and versioning of Android Components with the Mozilla release trains (GeckoView) to simplify release processes and integration by:

* Having just one component implementing `concept-engine` with GeckoView
* Having the `main` branch track only GeckoView Nightly
* Creating branches and releases tracking matching GeckoView releases (e.g. GeckoView 89.0 -> Android Components 89.0)

## Motivation

Initially we introduced three `concept-engine` implementations for GeckoView, one for each GeckoView release channel:

* `browser-engine-gecko-nightly`
* `browser-engine-gecko-beta`
* `browser-engine-gecko`

 This allowed us to release Android Components independently from GeckoView and consumers could use the latest AC release regardless of which GeckoView channel they wanted to use it with.

This flexibility comes at a cost and makes our release process and maintenance more difficult ([complex merge day process in the AC repository](https://github.com/mozilla-mobile/android-components/blob/d6fbb014667871504aadfdc712ca0313aabc1e46/docs/contribute/merge_day.md), multiple GeckoView components to maintain). For every new GeckoView release across all channels (nightly, beta, release) we want to ship a new release of Fenix (and with that Android Components). So there is already an implicit synchronization of releases, but it is manual and confusing. By making this dependency explicit through synchronized releases we can simplify our processes.

## Explanation

### One GeckoView component

Instead of having three components using GeckoView (`browser-engine-gecko-nightly`, `browser-engine-gecko-beta`, `browser-engine-gecko`), we will have just one (`browser-engine-gecko`) and on `main` it will track the latest GeckoView Nightly from `mozilla-central`.

### Releases

Nightly releases of Android Components will come with the latest Nightly version of GeckoView.

For every major version from the Mozilla repositories, we will have a major version release of Android Components. Those versions and releases will be synchronized across our whole stack: Fenix 89 will use Android Components 89, which will use GeckoView 89.

### Branches

On the Android Components side we will continue to have a `main` branch, which will track GeckoView Nightly and from which we will ship Android Components Nightly versions.

For every GeckoView version we will have a matching Android Components release branch (e.g. `releases/89.0`), that we cut from `main` the day the GeckoView Nightly version becomes Beta. This branch will continue to track the matching GeckoView version from beta builds to release builds.

### Merge day

On merge day the current Nightly version of GeckoView will become the new Beta version. At this point we will cut a matching Android Components release branch, which will continue to track the GeckoView version, while `main` will move on to track the next Nightly version.

Moving code between components will no longer be needed and the merge day procedure will be reduced to branching and versioning, which will be easier to automate.

Example: On the day GeckoView 89 Nightly becomes GeckoView Beta 89, we will cut an Android Components 89 release branch which will track GeckoView Beta 89 (which will eventually become the release version of GeckoView 89). The `main` branch of Android Components will continue to track GeckoView Nightly 90.

### Automation & Bots

We can continue to bump GeckoView and Android Component versions with our bots. The aligned version numbers simplify the process and even allow us to automate releasing and branching too.

* On `main` a bot will continue to update to the latest GeckoView Nightly version. With this change no other version bumps on `main` are needed.
* On a release branch (e.g. `releases/89.0`) a bot can update to the latest matching GeckoView version. Over time the branch will be bumped from Beta (e.g. 89.0 Beta) to Release versions of GeckoView (e.g. 89.0 release).

Additionally we could automate:

* Whenever the GeckoView Nightly major version changes on merge day and a new GeckoView Beta is available, a bot could cut a matching Android Components release branch and switch to the GeckoView Beta version (e.g. when GeckoView Nightly 89 switches to Nightly 90 a bot could cut a `releases/89.0` branch, bump it to GeckoView Beta 89 and cut a release).

### Localization

The localization workflow will be very similar to the process in Fenix. We would do the following:

* Frequently import strings to `main` through the `mozilla-l10n-bot`.
* Sync strings from `main` to `releases_v89.0` while `89` is in _Beta_. We would do this up to the merge day.

For the import to `main` no changes are needed. For the string sync between `main` and the _Beta branch_ we should be able to use the same code as we use for Fenix. (As of this writing, that code is in progress - having more similarity between Fenix and A-C would definitely simplify it.)

### Fenix integration

The integration into Fenix would look similar as today:
* `master` tracks AC Nightly with that GeckoView Nightly
* A release branch tracks the matching AC version. The aligned versions will avoid confusion (Fenix 89 release branch will use AC 89).

Fenix would still have three different product flavors: Nightly, Beta, Release. Those would continue to exist for supporting different branding and build configurations. However, the GeckoView version (and release channel) used by Fenix can only be controlled through the Android Component versions i.e., Fenix using AC 89 would always use GeckoView 89 regardless of product flavor.

In theory release branching (for code freeze and releases) could be automated on the Fenix side too with this change: For every major version bump (AC/GeckoView 89 -> 90) we could automatically cut the release branch and automate updating versions.

### Example

![](/assets/images/rfc/release-trains.png)

* `main` tracks GeckoView Nightly 89.0 (`browser-engine-gecko`)
* We ship AC 89.x Nightly versions from `main` every day
* On merge day:
  * We cut a `releases/89.0` release branch, which will continue to track GeckoView 89, now as a Beta version.
  * `main` continues with the next Nightly version (90.0)
* On the `releases/89.0` branch:
  * We update to the latest GeckoView 89.0 beta versions and eventually to GeckoView 89.0 release versions.
  * We ship AC 89.0.x versions from the release branch. Initially those will come with Beta versions of GeckoView 89.0 and eventually release versions.

## Drawbacks

* _Slower releases_: The initial design of having one component per GeckoView channel allowed independent and fast releases of Android Components. Aligning with the release trains of Mozilla repositories at first seems to cause slower releases. However at this time we do not require faster releases and create them for specific GeckoView releases to be used for specific Fenix releases already. In addition to that we can still uplift and release patches to skip trains - similar to how it is done in Mozilla repositories. It may feel more painful to uplift more complex patches. But at the same time this may be a good mechanism for identifying risky patches that should *not* skip the trains.

* _Versioning_: While we align on major versions, we will not be fully aligned at first. With the proposal above Android Components 89.0.0 will ship with a GeckoView 89 Beta version and eventually with a later patch version 89.0.x ship GeckoView 89.0 Release version. Right now we do not have any mechanism for marking an Android Components release as "Beta" version. This is something we could add in the future for dot releases. But since it is not required for *this* proposal, it is deliberately not covered here.

## Rationale and alternatives

* _Future Changes_: This simplified process will work nicely for our existing products (Fenix and Focus) and in theory should work for other apps too - especially if they have a dependency on GeckoView. However making the proposed changes does not prevent us from making changes in the future.

## Prior art & resources

* [Documentation: Current versioning and release process](https://mozac.org/contributing/versioning)
* [Documentation: Current merge day process](https://mozac.org/contributing/merge-day)
* [Firefox/GeckoView release calendar](https://wiki.mozilla.org/Release_Management/Calendar)
* [The Firefox/GeckoView release process](https://wiki.mozilla.org/Release_Management/Release_Process)
