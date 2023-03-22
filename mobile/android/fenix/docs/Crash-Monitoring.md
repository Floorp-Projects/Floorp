## Important
* The main goal here is not to file an issue for every single distinct crash report, but to find regressions of new problems that need to be addressed.
* Once you're familiar with the process this should not take more than 10 mins in the morning.
* Quick links that you should check everyday [Sentry Query](https://sentry.io/organizations/mozilla/issues/?groupStatsPeriod=auto&page=0&project=6295546&query=is%3Aunresolved+level%3Afatal+firstSeen%3A-1w&sort=freq&statsPeriod=14d) and [Socorro Query](https://crash-stats.mozilla.org/topcrashers/?product=Fenix&days=3&_range_type=build&process_type=any)

## Things to note before starting:
* Since we are focused on Java crashes, Sentry currently is the better choice.
* Ignore `level:info` issues (blue labels in Sentry).  These issues are informational only.

## What to do when monitoring crashes:
* Look at Sentry Fenix-nightly overview.  Go though trending issues. [Sentry Dashboard](https://sentry.io/organizations/mozilla/projects/fenix-nightly/?project=6295546).
* Look at Sentry custom search [Sentry Query](https://sentry.io/organizations/mozilla/issues/?groupStatsPeriod=auto&page=0&project=6295546&query=is%3Aunresolved+level%3Afatal+firstSeen%3A-1w&sort=freq&statsPeriod=14d).
* Sign up for https://groups.google.com/a/mozilla.org/g/stability to get a daily email on the stability of Fenix.

## How to determine a crash requires actions:
* Crashes that have level either Fatal or Error.
* Crashes that are occurring on latest Fenix and A-C versions.
* Crashes that are spiking.
* Crashes that are new.
* Crashes that happen repeatedly and often.
* Crashes that are happening to multiple users.

## When a crash report requires actions:
* Is this a crash due to a recent change? If so, contact the developer.
  * The histogram on the right side can help determine this along with checking the Firefox-Beta and Firefox Sentry products.
* Triage the crash to determine if the issue is real and requires a Bugzilla issue to track it. 
  * When filing an issue add a link to it as a comment in the Sentry crash for the products (nightly, beta, release) where the crash appears.
* Notify the relevant teams on Slack/Matrix that there's a new crash in Nightly that needs urgent attention, e.g. **#synced-client-integrations** for all things involving application services (A-S), **#nimbus-rust-sdk** for Nimbus, and **[GeckoView on Matrix](https://chat.mozilla.org/#/room/#geckoview:mozilla.org)**.

## What can you do to help when not monitoring crashes
* If you recently landed a new module / change that is significant, contact the crash monitor so they are aware of it.

## Crash monitoring with Socorro
* Look at [Top Crashers for Fenix Nightly](https://crash-stats.mozilla.org/topcrashers/?product=Fenix&days=3&_range_type=build&process_type=any) for reports on Nightly builds.
  * This will return zero results if GV build ID is greater than 3 days old. Change to the 7 day view and ask #releaseduty-mobile in Slack about the GV upgrade task being broken.
* Use Sentry and Bugzilla to determine if the crash has already been reported. If a Bugzilla bug has been filed for a crash, a link to the bug should be listed in Socorro's "Bugzilla IDs" column.
* If the crash is new and the volume is high, then consider filing an issue using the crash-stats Bugzilla tab from a crash ID.

## How to file bugs in Bugzilla from Socorro
* Go to the crash report.
* "Reports" tab.
* Select a report.
* Select the "Bugzilla" tab.
* Just above the header "Related Bugs", you will see a "Create a bug" option. Select the product that matches the crash report.
