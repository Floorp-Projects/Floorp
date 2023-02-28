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
TBD  | TBD | TBD | Upgrade Ubuntu 18.04 -> Ubuntu 22.04 X11
TBD  | TBD | TBD | Add Ubuntu 22.04 Wayland
TBD  | TBD | TBD | Upgrade Mac M1 from 11.2.3 -> 13.2.1
TBD  | TBD | TBD | replace 2017 acer perf laptops with lower end NUCs
TBD  | TBD | TBD | replace windows moonshots with mid level NUCs
TBD  | TBD | TBD | Upgrade android emulators to modern version


## Completed CI config changes

Start Date | Completed | Tracking Bug | Description
--- | --- | --- | ---
October 2022 | March 2023 | [Bug 1794900](https://bugzilla.mozilla.org/show_bug.cgi?id=1794900) | Migrate from win10 -> win11
November 2022 | February 2023 | [Bug 1804790](https://bugzilla.mozilla.org/show_bug.cgi?id=1804790) | Migrate Win7 unittests from AWS -> Azure
October 2022 | February 2023 | [Bug 1794895](https://bugzilla.mozilla.org/show_bug.cgi?id=1794895) | Migrate unittests from pixel2 -> pixel5
November 2020 | August 2021 | [Bug 1676850](https://bugzilla.mozilla.org/show_bug.cgi?id=1676850) | Windows tests migrate from AWS -> Datacenter/Azure and 1803 -> 20.04
May 2022 | July 2022 | [Bug 1767486](https://bugzilla.mozilla.org/show_bug.cgi?id=1767486) | Migrate perftests from Moto G5 phones to Samsung A51 phones
March 2021 | October 2021 | [Bug 1699541](https://bugzilla.mozilla.org/show_bug.cgi?id=1699541) | Migrate from OSX 10.14 -> 10.15
July 2020 | March 2021 | [Bug 1572739](https://bugzilla.mozilla.org/show_bug.cgi?id=1572739) | upgrade datacenter linux perf machines from ubuntu 16.04 to 18.04
September 2020  | January 2021 | [Bug 1665012](https://bugzilla.mozilla.org/show_bug.cgi?id=1665012) | Android phones upgrade from version 7 -> 10
October 2020  | February 2021 | [Bug 1673067](https://bugzilla.mozilla.org/show_bug.cgi?id=1673067) | Run tests on MacOSX Aarch64 (subset in parallel)
September 2020 | March 2021 | [Bug 1548264](https://bugzilla.mozilla.org/show_bug.cgi?id=1548264) | Python 2.7 -> 3.6 migration in CI
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
