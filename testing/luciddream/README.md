Luciddream is a test harness for running tests between a Firefox browser and another device, such as a Firefox OS emulator.

The primary goal of the project is to be able to test the Firefox Developer Tools' [remote debugging feature](https://developer.mozilla.org/en-US/docs/Tools/Remote_Debugging), where the developer tools can be used to debug web content running in another browser or device. Mozilla currently doesn't have any automated testing of this feature, so getting some automated tests running is a high priority.

The first planned milestone (for Q4 2014) is to stand up a prototype of a harness, in the process figuring out what the harness will look like and what tests will look like. This work is tracked in [bug 1064253](https://bugzilla.mozilla.org/show_bug.cgi?id=1064253) and is nearing completion. The current harness is based on the [Marionette Test Runner](https://developer.mozilla.org/en-US/docs/Marionette_Test_Runner), and tests right now are a subclass of [Marionette Python tests](https://developer.mozilla.org/en-US/docs/Mozilla/QA/Marionette/Marionette_Python_Tests).

The second planned milestone (for Q1 2015) is to get the harness running in Mozilla's continuous integration environment. Whether this will be in the legacy Buildbot environment or the new TaskCluster environment is yet to be determined. The bare minimum for this milestone will be having the tests run per-checkin against a Firefox Linux desktop build and a Firefox OS emulator both built from the same changeset. A stretch goal for this milestone will be to get tests running against a Firefox Linux desktop build per-checkin paired with stable release builds of the Firefox OS emulator, to be able to test backwards-compatibility of remote debugging. It's likely that as part of this work the repository of record will move from GitHub to mozilla-central, and the harness may be renamed from the current codename to a more descriptive (but less fun) name.

Future directions will likely include testing Firefox desktop remote debugging against other platforms, such as Firefox for Android, desktop Chrome, Chrome for Android, and Safari on iOS.

Points of Contact
=================

The primary developer of this project is Ted Mielczarek (@luser), ted on irc.mozilla.org, :ted in bugzilla.mozilla.org.

The primary DevTools point of contact is Alexandre Poirot (@ochameau), ochameau on irc.mozilla.org, :ochameau in bugzilla.mozilla.org.

Installation and Configuration
==============================

Currently running Luciddream is only supported on Linux, as the Firefox OS emulator is only well-supported there.

Install this module and its Python prerequisites in a virtualenv:
```
  virtualenv ./ve
  . ./ve/bin/activate
  python setup.py develop
```

[Download a Firefox build](http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-mozilla-central/) (if you don't already have one).


Download one of:
* [A Firefox OS emulator build](http://pvtbuilds.pvt.build.mozilla.org/pub/mozilla.org/b2g/tinderbox-builds/mozilla-central-emulator/) (if you don't already have one, this link requires Mozilla VPN access).
* [A B2G desktop build](http://ftp.mozilla.org/pub/mozilla.org/b2g/nightly/latest-mozilla-central/)


Unzip both your Firefox and your Firefox OS emulator/B2G desktop somewhere.

If you're on a 64-bit Ubuntu, you may need to do some fiddling to ensure you have the 32-bit OpenGL libraries available. See the "Solution : have both 32bit and 64bit OpenGL libs installed, with the right symlinks" section [in this blog post](http://rishav006.wordpress.com/2014/05/19/how-to-build-b2g-emulator-in-linux-environment/).


Running Tests
=============

To run with a Firefox OS emulator:
```
runluciddream --b2gpath /path/to/b2g-distro/ --browser-path /path/to/firefox/firefox example-tests/luciddream.ini
```

To run with B2G desktop:
```
runluciddream --b2g-desktop-path /path/to/b2g/b2g --browser-path /path/to/firefox/firefox example-tests/luciddream.ini
```

If you're using a locally-built B2G desktop build which doesn't have a Gaia profile included you should get Gaia and build a profile, and then pass that in with the `--gaia-profile` option:
```
runluciddream --b2g-desktop-path /path/to/obj-b2g/dist/bin/b2g --gaia-profile /path/to/gaia/profile --browser-path /path/to/firefox/firefox example-tests/luciddream.ini
```
