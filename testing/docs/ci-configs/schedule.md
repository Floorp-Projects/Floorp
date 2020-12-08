# Schedule

For each CI config change, we need to follow:
 * scope of work (what will run, how frequently)
 * capacity planning (cost, physical space limitations)
 * will this replace anything or is this 100% new
 * puppet/deployment scripts or documentation
 * setup pool on try server
 * documented updated on this page, communicate with release management and others as appropriate


## Current / Future CI config changes

Start Date | Completed | Tracking Bug | Description
--- | --- | --- | ---
October 2020  | TBD | [Bug 1665012](https://bugzilla.mozilla.org/show_bug.cgi?id=1665012) | add samsung S7 phones for perf testing
November 2020 | TBD | [Bug 1676850](https://bugzilla.mozilla.org/show_bug.cgi?id=1676850) | Windows tests migrate from AWS -> Datacenter/Azure and 1803 -> 1903
November 2020 | TBD | TBD | upgrade datacenter linux perf machines from ubuntu 16.04 to 18.04
TBD  | TBD | [Bug 1665012](https://bugzilla.mozilla.org/show_bug.cgi?id=1665012) | Android phones upgrade from version 7 -> 10
October 2020  | TBD | [Bug 1673067](https://bugzilla.mozilla.org/show_bug.cgi?id=1673067) | Run tests on MacOSX BigSur (subset in parallel)
October 2020  | TBD | [Bug 1673067](https://bugzilla.mozilla.org/show_bug.cgi?id=1673067) | Run tests on MacOSX Aarch64 (subset in parallel)
December 2020 | TBD | TBD | Migrate OSX from Mac Mini R7, OSX 10.14 (Mojave) -> Mac Mini R8, OSX 10.15 (Catalina)
TBD  | TBD | TBD | Migrate more coverage of OSX from 10.14 to BigSur/aarch64
TBD  | TBD | TBD | Upgrade ubuntu from 18.04 to 20.04
TBD  | TBD | TBD | Upgrade android emulators to modern version
September 2020 | TBD | [Bug 1548264](https://bugzilla.mozilla.org/show_bug.cgi?id=1548264) | Python 2.7 -> 3.6 migration in CI
TBD  | TBD | [Bug 1665010](https://bugzilla.mozilla.org/show_bug.cgi?id=1665010) | Add more android phone hardware (replace moto g5 and probably pixel 2)
TBD  | TBD | TBD | Upgrade datacenter hardware for windows/linux (primarily perf)
TBD  | TBD | TBD | Add Linux ARM64 worker in AWS (as it is close to Apple Silicon)


## Completed CI config changes

Start Date | Completed | Tracking Bug | Description
--- | --- | --- | ---
July 2020 | October 2020| [Bug 1653344](https://bugzilla.mozilla.org/show_bug.cgi?id=1653344) | Remove EDID dongles from MacOSX machines
August 2020 | September 2020 | [Bug 1643689](https://bugzilla.mozilla.org/show_bug.cgi?id=1643689) | Schedule tests by test selection/manifest
June 2020 | August 2020 | [Bug 1486004](https://bugzilla.mozilla.org/show_bug.cgi?id=1486004) | Android hardware tests running without rooted phones
August 2019 | January 2020 | [Bug 1572242](https://bugzilla.mozilla.org/show_bug.cgi?id=1572242) | Upgrade Ubuntu from 16.04 to 18.04 (finished in January)


## Appendix:
 * *OS*: base operating system such as Android, Linux, Mac OSX, Windows
 * *Hardware*: specific cpu/memory/disk/graphics/display/inputs that we are using, could be physical hardware we own or manage, or it could be a cloud provider.
 * *Platform*: a combination of hardware and OS
 * *Configuration*: what we change on a platform (can be runtime with flags), installed OS software updates (service pack), tools (python/node/etc.), hardware or OS settings (anti aliasing, display resolution, background processes, clipboard), environment variables, 
 * *Test Failure*: a test doesn’t report the expected result (if we expect fail and we crash, that is unexpected).  Typically this is a failure, but it can be a timeout, crash, not run, or even pass
 * *Greening up*: Assuming all tests return expected results (passing), they are green.  When tests fail, they are orange.  We need to find a way to get all tests green by investigating test failures.
