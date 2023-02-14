## Overview ##

Firefox for Android roughly follows the [Firefox Gecko release schedule](https://wiki.mozilla.org/Release_Management/Calendar#Calendars).
This means we cut a Beta every 4 weeks, with a full cycle (~4 weeks) of baking on Beta before going to Production release. 

The [Firefox for Android release schedule](https://docs.google.com/spreadsheets/d/1HotjliSCGOp2nTkfXrxv8qYcurNpkqLWBKbbId6ovTY/edit#gid=0) contains more details related to specific Mobile handoffs.

### Requirements
- JIRA access
- Bugzilla account
- Sentry access

## Firefox for Android Release
There are two releases this covers: the current changes in the Fenix Nightly channel that is going out to Beta, and the current Beta that is going to Production.

## Cutting a Beta

- [ ] Review [FeatureFlags](https://github.com/mozilla-mobile/fenix/blob/main/app/src/main/java/org/mozilla/fenix/FeatureFlags.kt) to determine if there are features that need to be enabled (or disabled) for Beta and Production release of Fenix. This will be a discussion with PO, PMs, EMs.
- [ ] Make a new Beta: Follow instructions [here](https://github.com/mozilla-mobile/fenix/wiki/Creating-a-release-branch) and notify the Release Management team (slack: #releaseduty-mobile). QA team is notified that a Beta release has been captured and they will run tests for Beta release sign-off
- [ ] Once there is GREEN QA signoff, the Release Management team (slack: #releaseduty-mobile) pushes the Beta version in the [Google Play Console](https://play.google.com/console/)
- [ ] Check Sentry each day for issues on [Firefox Beta](https://sentry.prod.mozaws.net/operations/firefox-beta/) and if nothing concerning, Release Management team bumps releases to 25%. Subsequent Beta builds are bumped to 100% assuming no blocking issues arise.
### Bugfix uplifts / Beta Product Integrity 
- [ ] If bugs are considered release blocker then find someone to fix them on main and the milestone branch (cherry-pick / uplift)
    - [ ] Add the uplift request to the appropriate row in the [Uplifts document](https://docs.google.com/spreadsheets/d/1qIvHpcQ3BqJtlzV5T4M1MhbWVxkNiG-ToeYnWEBW4-I/edit#gid=0). Ask for approval of uplift from Release Owner [amedyne](https://github.com/amedyne) and then notify Release Management team (slack: #releaseduty-mobile) of the uplift changes
- Note: Beta release versions are captured at least once a week during the Beta cycle.


### Production Release Candidate 
- Production Release Candidate is captured on the third week of Beta by the Release Management team (slack: #releaseduty-mobile). This is then sent to Quality Assurance for Production Release Testing Sign-off.  


## Production Release
- [ ] Once there is GREEN QA signoff, the Production Release Candidate is pushed to the Alpha testing track in [Google Play Console](https://play.google.com/console/u/0/developers/7083182635971239206/app/4972519468758466290/releases/overview) by the Release Management team (slack: #releaseduty-mobile)
- [ ] If nothing is concerning, release management officially tags the Release Candidate as Production release, (usually 1 week after 1st Release Candidate)
- [ ] Check Sentry for new crashes. Follow instructions for [Crash Monitoring](https://github.com/mozilla-mobile/fenix/wiki/Crash-Monitoring). File issues and triage.
- [ ] If nothing concerning, release management bumps releases(5%, 25%, 100%)

