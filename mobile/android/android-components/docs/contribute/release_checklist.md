---
layout: page
title: Release checklist
permalink: /contributing/release-checklist
---

## Before the release

- Make sure version number is correct or update version in Config.kt 
- Update [CHANGELOG](https://github.com/mozilla-mobile/android-components/blob/master/docs/changelog.md)
  - Use milestone and commit log for identifying interesting changes
- Generate API docs for release
  - In the repository root run: `./automation/docs/generate_api-reference_release.sh`
- Close milestone
  - Milestone should be 100% complete. Incomplete issues should have been moved in planning meeting.

## Release

- Create a new tag/release on GitHub.
  - Tag: v2.0 (_v_ prefix!)
  - Release 2.0
- Wait for taskcluster to complete successfully
- Check that new versions are on jcenter:
  - [Components on jcenter](http://jcenter.bintray.com/org/mozilla/components/)
- Request new packages to be linked to jcenter (if there are any new ones)

## After the release

- Send release message to Slack
```
*Android Components 0.17 Release* (:ocean:)
Release notes: https://mozilla-mobile.github.io/android-components/changelog/
Milestone: https://github.com/mozilla-mobile/android-components/milestone/15?closed=1
```
- Send release message to mailing lists
  - android-components@lists.mozilla.org, mobile-all@mozilla.com, epm-status@mozilla.com
  - Call out other interesting news, e.g. "Project X started using component Y"
- Update version number in repository
  - [build.gradle](https://github.com/mozilla-mobile/android-components/blob/master/build.gradle#L29)
