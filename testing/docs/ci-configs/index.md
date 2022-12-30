# Configuration Changes

This process outlines how Mozilla will handle configuration changes.  For a list of configuration changes, please see the [schedule](schedule.md)

## Infrastructure setup (2-4 weeks)

This is behind the scenes, when there is a need for a configuration change (upgrade or addition of a new platform), the first step
is to build a machine and work to get the OS working with taskcluster.  This is work for hardware/cloud is done by IT.  Sometimes
this is as simple as installing a package or changing an OS setting on an existing machine, but this requires automation and documentation.

In some cases there is little to no work as the CI change is running tests with different runtime settings (environment variables or preferences).


## Setting up a pool on try server (1 week)

The next step is getting some machines available on try server.  This is where we add some code in tree to support the new config
(a new worker type, test variant, etc.) and validate any setup done by IT works with taskcluster client.  Then Releng ensures the target tests
can run at a basic level (mozharness, testharness, os environment, logging, something passes).


## Green up tests (1 week)

This is a stage where Releng will run all the target tests on try server and disable, skip, fail-if all tests that are not passing or frequently
intermittent.  Typically there are a dozen or so iterations of this because a crash on one test means we don't run the rest of the tests in the
manifest.


## Turn on new config as tier-2 (1/2 week)

We will time this at the start of a new release.

Releng will land changes to manifests for all non passing tests and then schedule the new jobs by default.  This will be tier-2 for a couple reasons:
 * it is a new config with a lot of tests that still need attention
 * in many cases there is a previous config (lets say upgrading windows 10 from 1803 -> 1903) which is still running in parallel as tier-1

This will now run on central and integration and be available on try server.  In a few cases where there are limited machines (android phones),
there will be needs to turn off the old config, or make the try server access hidden behind `./mach try --full`


## Turn on new backstop jobs which run the skipped tests (1/2 week)

Releng will turn on a new temporary job that will run the tests which are not green by default.  These will run as tier-2 on mozilla-central and be sheriffed.

The goal here is to find tests that are now passing and should be run by default.  By doing this we are effectively running all the tests instead of
disabling dozens of tests and forgetting about them.


## Handoff to developers (1 week)

Releng will file bugs for all failing tests (one bug per manifest) and needinfo the triage owner to raise awareness that one or more tests in their area need
attention.  At this point, Releng is done and will move onto other work.  Developers can reproduce the failures on try server and when fixed edit the manifest
as appropriate.

There will be at least 6 weeks to investigate and fix the tests before they are promoted to tier-1.


## move config to tier-1 (6-7 weeks later)

After the config has been running as tier-2 makes it to beta and then to the release branch (i.e. 2 new releases later), Releng will:
 * turn off the old tier-1 tests (if applicable)
 * promote the tier-2 jobs to tier-1
 * turn off the backstop jobs

This allows developers to schedule time in a 6 weeks period to investigate and fix any test failures.
