## Overview ##

| Monday          | Tuesday                   | Wednesday                    | Thursday       | Friday      |
|-----------------|---------------------------|------------------------------|----------------|-------------|
| (Sprint 1 Start)|                           |                              |                |             |
|                 | Hard code freeze for Beta |                              |                | Code Freeze/Planning
| Sprint 2 Start / Release to Beta / Release Production in Play Store 1% | QA Beta / Promote Release 25%  | Promote Release 100%  |      |      |

### Requirements
- Jira account
- Bugzilla account
- Google Play access (for reviewing crashes)

## Release Checklist
There are two releases this covers: the current sprint that is going out to Beta, and the previous beta that is going to Production.
We will refer to the beta release going out as the *current* sprint release.

## Start of sprint [Monday, 1st week of sprint]
- [ ] Create milestone for *upcoming* sprint release. (e.g. if you are doing releng for v2.2, create v2.3 milestone)
- [ ] If the upcoming release is a *major* (x.0) release, create an issue in the *upcoming* milestone: "What's New Entry for [*upcoming* release]" to track work for the SUMO page and Google Play release notes, e.g., if the current release is 2.3 but the upcoming one will be 3.0, make a "What's New" issue for 3.0. Product will use this to remember to check in with SUMO.
- [ ] [Create an issue](https://github.com/mozilla-mobile/fenix/issues/new?template=release_checklist.md&title=Releng+for+) in the *upcoming* milestone: "Releng for v[release]". Find an engineer who will handle the next releng task and assign them.

## Release Day [Monday, 3rd week] Beta & Production
- [ ] Promote previous Beta to Release
    - [ ] Cherry-pick all merged [automated L10N string PRs](https://github.com/mozilla-mobile/fenix/pull/6156) to add newly translated strings and open a PR against branch that is going to release. (TODO is this safe?) This will require review.
    - [ ] Tag the latest released RC version additionally with the tag of the release (v1.0-RC2 -> v1.0) (This can be done as soon as there are no more release blockers, does not need to be on Release Day.)
      - [ ] **Verify that the commit hash of the new release matches the most recent RC.**  This ensures that the correct version will be released
    - [ ] Create a GitHub release build `vX.X.X` (v2.3.0) with the previous Beta branch as the target.
    - [ ] Smoketest the signed build
      - [ ] Load a url
      - [ ] Set up sync
      - [ ] Delete browsing data
    - [ ] Upload the signed APK from the Taskcluster `signing-production` task to the [release page](https://github.com/mozilla-mobile/fenix/releases)
    - [ ] Create a release request in Bugzilla to release to 1%. You can clone [this issue](https://bugzilla.mozilla.org/show_bug.cgi?id=1571967) and `need-info` someone from release management.
- [ ] Make a new Beta
    - [ ] Create a branch off of master (DO NOT PUSH YET) for the *current* milestone of format `releases/v2.3` (where 2.3 is the *current* milestone). After that, anything landing in master will be part of the next milestone.
    - [ ] On the new Beta branch, pin the AC version to the stable version ([example](https://github.com/mozilla-mobile/fenix/commit/e413da29f6a7a7d4a765817a9cd5687abbf27619)) with commit message "Issue #`<this releng issue>`: Pin to stable AC `<version>` for release v2.3" (replacing 2.3 with the version)
    - For each issue closed since the last release (run `kotlinc -script automation/releasetools/PrintMentionedIssuesAndPrs.kts` to get a list [see script for details] and paste it into the Releng issue):
      - [ ] Ensure it has the correct milestone.
      - [ ] Add `eng:qa:needed` flags on each issue that still needs it.
    - [ ] Go through the list of issues closed during this sprint in the Done column of the [Sprint Kanban](https://github.com/mozilla-mobile/fenix/projects/9) and make sure they all have the correct milestone.
    - Note: You will need code review to make changes to the release branch after this point, because it is a protected branch.
        - [ ] Push the branch.

    - [ ] Create a GitHub pre-release build `vX.X.X-beta.1` (v2.3.0-beta.1)  with the release branch as the target. This will kick off a build of the branch. You can see it in the mouseover of the CI badge of the branch in the commits view. Builds are found under `signing-*` task.
    - If you need to trigger a new RC build, you will need to draft and publish a new (pre-release) release. Editing an existing release and creating a new tag will not trigger a new RC build.

    - [ ] Create a new PI (product integrity) request in Jira. You can clone [this issue](https://jira.mozilla.com/browse/PI-219).

### SUMO Verification [After Beta release]
- [ ] If the *current* release is a major (x.0) release, review the SUMO article contents of the whats new / other sumo pages and make sure they are accurate with what is in this release. If not, escalate to Product Owner.

### During Beta Product Integrity (Beta Release until PI green signoff)
- [ ] If bugs are considered release blocker then find someone to fix them on master and the milestone branch (cherry-pick / uplift)
- [ ] If needed tag a new RC version (e.g. v1.0-RC2) and follow the submission checklist again.

### During Production Release Rollout [Tuesday, Wednesday following Monday Release Day]
- [ ] Check Sentry for new crashes. File issues and triage.
- [ ] Ask Relman in the bug if they see potential blockers on Google Play, and if not, request that they bump the release each day (25% Tu, 100% Wed)

    Major releases often need to be synchronized with other marketing activities (e.g. blog postings).

## Room for improvement
- [ ] Automate assigning milestones to closed issues (based on date, etc) #6199
- [ ] Automate assignig `eng:qa:needed` to issues #6199
- [ ] Automate verification that the commit hash matches the most recent RC
- [ ] Builds generated as part of `signing-production` task look like `public/build/arm64-v8a/geckoBeta/target.apk`.  This means that the dev must download, then rename them by hand.  Could RM update these to generate `public/build/arm64-v8a/geckoBeta/firefox-preview-v3.0.0-rc.1-arm64-v8a.apk`, or similar?
