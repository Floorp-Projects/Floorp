## Overview ##
Code freeze for a milestone is the *second Friday of the sprint*, and most of releng happens between *Monday and and Thursday (of the next milestone)*, with the *Beta release Thursday*. Beta should be promoted to Release on the release date. String freeze is the *first Thursday* of the sprint, but can be pushed to the *second Wednesday* of the sprint if cleared with Delphine/L10N.

## Sprint start [1st week of milestone]

- [ ] Update versionName in build.gradle for the next milestone (preliminary version number)

## String freeze and string Export [Thursday, 1st week of milestone]
- [ ] Make sure the "Final string Import" from the previous release has already happened. (Check with the RM from the last release if it was not been checked off) This is to prevent string changes (mainly changes or deletions) from the this release from getting translated and included in the string import for the previous release (where the feature set is different, and the linter will fail).
- [ ] Make sure all issues with label "strings needed" have strings assigned and the label updated to "strings approved"
- [ ] Pre-land all strings for features that have not been implemented yet
- [ ] Export strings and open PR in the L10N repository
- [ ] If there are strings that haven't been pre-landed, land them **no later** than *second Wednesday* of the sprint and give a heads up to Delphine

## Feature complete [Monday, 1st week of next milestone]

- [ ] Create a branch for the *current* milestone and protect it through Settings on the repo (need admin privileges). After that master is tracking the next milestone. Usually done on the Monday after feature complete on Friday.
- [ ] Create an issue in the *upcoming* milestone: "What's New Entry for [release]" to track work for the SUMO page ([example](https://github.com/mozilla-mobile/focus-android/issues/1670)).
- [ ] [Create an issue](https://github.com/mozilla-mobile/focus-android/issues/new?template=release_checklist_template.md&title=Releng+for+) in the *upcoming* milestone: "Releng for [release]" and copy this checklist into it. Assign the next assignee.
- [ ] Go through the list of bugs closed during this sprint and make sure all they're all added to the correct milestone.
- [ ] Make sure new What's New Link is updated in SupportUtils for release
- [ ] Add either `ReadyForQA` or `QANotNeeded` flags on each of the bugs in the current milestone.

## Final string Import [Wednesday, 1st week of next milestone]
- [ ] Cherry pick any new string import commits from `master` into the release branch

## Beta Submission [Thursday, 1st week of next milestone]

- [ ] Tag first RC version in Github. For 1.0 the tag would be v1.0-RC1. This will kick off a build of the branch. The format for the Taskcluster task is `https://github.taskcluster.net/v1/repository/mozilla-mobile/focus-android/<milestone-branch>/latest`, or you can see it in the mouseover of the CI badge of the branch in the commits view.

    If you need to trigger a new RC build, you need to draft a publish a new (pre-release) release. Editing an existing release and creating a new tag will not trigger a new RC build.

- [ ] Create a [Bugzilla bug](https://bugzilla.mozilla.org/enter_bug.cgi?format=__default__&product=Release%20Engineering&cloned_bug_id=1408386) for signing and uploading the builds [Friday after code release]. Include the planned dates for Beta and Release release, from the [train schedule](https://wiki.mozilla.org/Mobile/Focus/Android/Train_Schedule).
- [ ] Let release engineering sign the builds and upload the signed builds to Bugzilla
- [ ] Let release management upload the builds to the Alpha channel (internal distribution)
- [ ] If the builds passed a smoke test let release management promote the build to the beta channel (public distribution)
- [ ] Upload the APK to the [releases page](https://github.com/mozilla-mobile/focus-android/releases).
- [ ] Upload the APK to archive.mozilla.org into a folder with the name of the version (e.g. /android/focus/2.2-RC1) (S3 bucket)

## During Beta

- [ ] Check Google Play for new crashes. File issues and triage.
- [ ] If bugs are considered release blocker then fix them on master and the milestone branch (cherry-pick / uplift)
- [ ] If needed tag a new RC version (e.g. v1.0-RC2) and follow the submission checklist again.

## Release

- [ ] Release management promotes the build from beta to the release channel. For minor releases this can happen at any time during the day. Major releases often need to be synchronized with other marketing activities (e.g. blog postings). Releases are rolled out to 99% of the population (otherwise the rollout can't be stopped).

## After the release

- [ ] After some days release management sets the roll out to 100%
- [ ] Check whether there are new crashes that need to be filed.
- [ ] Tag the last and released RC version additionally with the tag of the release (v1.0-RC2 -> v1.0)
- [ ] Upload signed APKs to the [release page](https://github.com/mozilla-mobile/focus-android/releases)
- [ ] Upload the APK to http://archive.mozilla.org/pub/android/focus/ (S3 bucket) into a folder named like a release (e.g. /android/focus/2.2). Builds from the "latest" folder are linked from the mozilla.org website.

```
aws configure
aws s3 cp <klar-or-focus-apk-path> s3://net-mozaws-prod-delivery-archive/pub/android/<new-version>/
```
Also replace the APKs in /android/focus/latest/. Builds from the "latest" folder are linked from the mozilla.org website.