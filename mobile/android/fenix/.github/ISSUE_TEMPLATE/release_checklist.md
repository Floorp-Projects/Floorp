## Overview ##

Firefox for Android roughly follows the [Firefox Gecko release schedule](https://wiki.mozilla.org/Release_Management/Calendar#Calendars).
This means we cut a beta at the end of every two sprints, with a full cycle (~4 weeks) of baking on Beta before going to release. Uplifts must be approved by Release Owner (st3fan).

The [Firefox for Android release schedule](https://docs.google.com/spreadsheets/d/1HotjliSCGOp2nTkfXrxv8qYcurNpkqLWBKbbId6ovTY/edit#gid=0) contains more details related to specific Mobile handoffs.

| Monday          | Tuesday                   | Wednesday                      | Thursday          | Friday          |
|-----------------|---------------------------|--------------------------------|-------------------|-----------------|
| (week 1)        |                           | (Y.2 sprint ended)             |Sprint **X.1** starts |              |
| (week 2)        |                           |                                |                   |                 |
| (week 3)        |                           | Cut **X.1-beta** 12PST         | **X.1-beta** QA / Sprint X.2 starts ||
| (week 4)        |                           |                                |                   |                 |
| (week 5)        |                           | Uplift L10N to **X.1-beta**    | Sprint Z.1 starts |                 |
| (week 6)        | Build X.1-RC with GV Prod for QA |                         |                   |                 |
| (week 7)        | Release X.1 - 5%          | Release X.1 20% / Cut Z.1-beta | Release X.1 100%  |                 |

### Requirements
- JIRA access
- Bugzilla account
- Sentry access

## Release Checklist
There are two releases this covers: the current sprint that is going out to Beta, and the previous Beta that is going to Production.

## Start of Sprint X.1 [Thursday, 1st week of sprint]
- [ ] [Create an issue](https://github.com/mozilla-mobile/fenix/issues/new?template=release_checklist.md&title=Releng+for+) "Releng for v[release]" to track the current sprint.

## Sprint X.1 End [Wednesday, 2nd week] Cutting a Beta 
- [ ] Make a new Beta
    - [ ] Create a branch off of master (DO NOT PUSH YET) for the *current* milestone of format `releases/v85.0.0`.  After that, anything landing in master will be part of the next release.
    - [ ] On the new Beta branch, pin the AC version to the stable version ([example](https://github.com/mozilla-mobile/fenix/commit/e413da29f6a7a7d4a765817a9cd5687abbf27619)) with commit message "Issue #`<this releng issue>`: Pin to stable AC `<version>` for release v85"
        - [ ] Update the title to include this AC version "Releng for v[release] with AC [version]"
    - Note: You will need code review to make changes to the release branch after this point, because it is a protected branch.
        - [ ] Push the branch.

    - [ ] Create a GitHub pre-release [Release](https://github.com/mozilla-mobile/fenix/releases) with:
        - [ ] Tag of the format `vX.X.X-beta.1` (v85.0.0-beta.1)
        - [ ] The Target branch is the release branch (releases/v85.0.0)
        - [ ] For the description of the release, look at the [Jira boards](https://jira.mozilla.com/secure/RapidBoard.jspa?rapidView=299&projectKey=FNX&view=reporting&chart=sprintRetrospective&sprint=883) for the X.1 and previous Y.2 sprints and list the major features that were added. This will help with the release notes later on.
        - [ ] Click "Publish release". This will kick off a build of the branch. You can see it in the mouseover of the CI badge of the branch in the commits view. Builds are found under `signing-*` task.
                - If you need to trigger a new RC build, you **MUST** draft and publish a new (pre-release) release (optionally deleting both the release and the tag). Editing an existing release and creating a new tag will **not** trigger a new build.
    - [ ] Send an email to QA at mozilla-mobile-qa@softvision.com with a link to the Taskcluster build (subdirectory of the [Fenix CI](https://firefox-ci-tc.services.mozilla.com/tasks/index/mobile.v2.fenix.beta))

### Bugfix uplifts / Beta Product Integrity (Beta Release until PI green signoff)
- [ ] If bugs are considered release blocker then find someone to fix them on master and the milestone branch (cherry-pick / uplift)
    - [ ] Add the uplift request to the appropriate row in the [Uplifts document](https://docs.google.com/spreadsheets/d/1qIvHpcQ3BqJtlzV5T4M1MhbWVxkNiG-ToeYnWEBW4-I/edit#gid=0).
- [ ] If needed tag a new beta version (e.g. v1.0-beta.2) and follow the submission checklist again.
- [ ] Once there is GREEN QA signoff, file a [release management bugzilla for rollout](https://bugzilla.mozilla.org/show_bug.cgi?id=1664366)
    - [ ] Check Sentry each day for issues on [Firefox Beta](https://sentry.prod.mozaws.net/operations/firefox-beta/) and if nothing concerning, bump release in the bugzilla (5%, 20%, 100%)

### Uplifting L10N strings to Beta [Wednesday, 2 weeks after sprint end]
- [ ] Find the issue ([example](https://github.com/mozilla-mobile/fenix/issues/16381)) filed by L10N / delphine saying string are ready for uplift (it takes 2 weeks for localizers to prepare localization).
- [ ] If there are new locales that are ready to be added to Release, add them to [l10n-release.toml](https://github.com/mozilla-mobile/fenix/blob/master/l10n-release.toml)
- [ ] Run the [L10N uplift script](https://github.com/mozilla-mobile/fenix/blob/master/l10n-uplift.py) against the releases/vX.1 branch (releases/v85.0.0). There will likely be conflicts, but if you are confused, they should match the strings in [main/Nightly](https://github.com/mozilla-mobile/fenix/tree/master/app/src/main/res)
- [ ] Once all conflicts are resolved, tag a new Beta to be released.
- [ ] Notify delphine in the L10N issue that the strings have been uplifted, and string quarantine can be lifted

### Production Release Candidate [Tuesday, 3 weeks after X.1 Beta was cut]
- [ ] In android-components: Create a dot release with the GeckoView Production release candidate.
- [ ] Open a PR against the release branch (releases/v85.0.0) with the AC version bump "Pin to stable AC `<version>` for release v85`. You will need code review.
- [ ] Create a GitHub pre-release [Release](https://github.com/mozilla-mobile/fenix/releases) with:
    - [ ] Tag of the format `vX.X.X-rc.1` (v85.0.0-rc.1)
    - [ ] The Target branch is the release branch (releases/v85.0.0)
    - [ ] For the description, copy the beta description
- [ ] Send an email to QA at mozilla-mobile-qa@softvision.com with a link to the Taskcluster build (subdirectory of the [Fenix CI](https://firefox-ci-tc.services.mozilla.com/tasks/index/mobile.v2.fenix.release))

### Production Release [Release day, from [release calendar](https://docs.google.com/spreadsheets/d/1HotjliSCGOp2nTkfXrxv8qYcurNpkqLWBKbbId6ovTY/edit#gid=0)]
- [ ] Create a GitHub [Release](https://github.com/mozilla-mobile/fenix/releases) with:
    - [ ] Tag of the format `vX.1.X` (v85.1.0) (increment the minor version for new cuts)
    - [ ] The Target branch is the release branch (releases/v85.0.0)
    - [ ] For the description, copy the beta description
    - [ ] file Bugzilla ticket for [release manament](https://bugzilla.mozilla.org/show_bug.cgi?id=1672212)

- [ ] Check Sentry for new crashes. File issues and triage.
- [ ] Each day, bump the release rollout if nothing concerning (5%, 20%, 100%)
