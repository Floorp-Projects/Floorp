## Overview ##
Soft code freeze for a release is the *second Wednesday of the sprint*, and most of releng happens between *Wednesday and Friday*, String freeze is the *first Tuesday* of the sprint.

| Monday       | Tuesday     | Wednesday                    | Thursday       | Friday      |
|--------------|-------------|------------------------------|----------------|-------------|
| Sprint Start | Sprint Work | Sprint Work                  | Sprint Work    | Sprint Work |
| Sprint Work  | Sprint Work | Sprint Work/Soft Code Freeze | Sprint Work/QA | Planning    |
|              | Release     |                              |                |             |

## Start of sprint
- [ ] Create an issue in the *upcoming* milestone: "What's New Entry for [release]" to track work for the SUMO page and Google Play release notes.

## String freeze
- [ ] Make sure all issues with label "strings needed" have strings assigned and the label updated to "strings approved"
- [ ] Pre-land all strings for features that have not been implemented yet

## Soft code freeze [Wednesday, 2nd week of sprint]
- [ ] Create a branch for the *current* milestone and protect it through Settings on the repo (need admin privileges). After that master is tracking the next milestone.
- [ ] [Create an issue](https://github.com/mozilla-mobile/fenix/issues/new?template=release_checklist.md&title=Releng+for+) in the *upcoming* milestone: "Releng for [release]".
- [ ] Go through the list of bugs closed during this sprint and make sure all they're all added to the correct milestone.
- [ ] Add either `eng:qa:needed` flags on each issue that still needs it.
- [ ] Make sure the "Final string Import" has already happened.

## Product Integrity

- [ ] Tag first pre-release RC version in Github. For 1.0 the tag would be 1.0.0-rc.1. This will kick off a build of the branch. You can see it in the mouseover of the CI badge of the branch in the commits view.

    If you need to trigger a new RC build, you need to draft a publish a new (pre-release) release. Editing an existing release and creating a new tag will not trigger a new RC build.

- [ ] Upload the APK from the Taskcluster signing task to the [releases page](https://github.com/mozilla-mobile/fenix/releases).
- [ ] Create a new PI request in Jira. You can clone [this issue](https://jira.mozilla.com/browse/PI-219).

## During Product Integrity

- [ ] Check Google Play for new crashes. File issues and triage.
- [ ] If bugs are considered release blocker then fix them on master and the milestone branch (cherry-pick / uplift)
- [ ] If needed tag a new RC version (e.g. v1.0-RC2) and follow the submission checklist again.

## Release

- [ ] Tag the last and released RC version additionally with the tag of the release (v1.0-RC2 -> v1.0)
- [ ] Upload signed APKs to the [release page](https://github.com/mozilla-mobile/fenix/releases)
- [ ] Create a release request in Bugzilla. You can clone [this issue](https://bugzilla.mozilla.org/show_bug.cgi?id=1571967) and NI someone from release management.

    Release management promotes the build from beta to the release channel. For minor releases this can happen at any time during the day. Major releases often need to be synchronized with other marketing activities (e.g. blog postings). Releases are rolled out to 99% of the population (otherwise the rollout can't be stopped).

## After the release

- [ ] Follow up with release management to set the rollout to 100%
- [ ] Check whether there are new crashes that need to be filed.

