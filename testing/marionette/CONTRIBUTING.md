Contributing
============

We are delighted that you want to help improve Marionette!
‘Marionette’ means different a few different things, depending
on who you talk to, but the overall scope of the project involves
these components:

  * [_Marionette_] is a Firefox remote protocol to communicate with,
    instrument, and control Gecko-based browsers such as Firefox
    and Fennec.  It is built in to Firefox and written in [XPCOM]
    flavoured JavaScript.

    It serves as the backend for the geckodriver WebDriver implementation,
    and is used in the context of Firefox UI tests, reftesting,
    Web Platform Tests, test harness bootstrapping, and in many
    other far-reaching places where browser instrumentation is required.

  * [_geckodriver_] provides the HTTP API described by the [WebDriver
    protocol] to communicate with Gecko-based browsers such as
    Firefox and Fennec.  It is a standalone executable written in
    Rust, and can be used with compatible W3C WebDriver clients.

  * [_webdriver_] is a Rust crate providing interfaces, traits
    and types, errors, type- and bounds checks, and JSON marshaling
    for correctly parsing and emitting the [WebDriver protocol].

By participating in this project, you agree to abide by the Mozilla
[Community Participation Guidelines].  Here are some guidelines
for contributing high-quality and actionable bugs and code.

[_Marionette_]: ./README.md
[_geckodriver_]: ../geckodriver/README.md
[_webdriver_]: ../webdriver/README.md
[WebDriver protocol]: https://w3c.github.io/webdriver/webdriver-spec.html#protocol
[XPCOM]: https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XPCOM/Guide
[Community Participation Guidelines]: https://www.mozilla.org/en-US/about/governance/policies/participation/


Writing code
============

Because there are many moving parts involved remote controlling
a web browser, it can be challenging to a new contributor to know
where to start.  Please don’t hesitate to [ask questions]!

The canonical source code repository is [mozilla-central].  Bugs are
filed in the [`Testing :: Marionette`] component on Bugzilla.  We also
have a curated set of [good first bugs] you may consider attempting first.

The purpose of this guide _is not_ to make sure you have a basic
development environment set up.  For that there is plentiful
documentation, such as the [Developer Guide] to get you rolling.
Once you do, we can get started working up your first patch!
Remember to [reach out to us] at any point if you have questions.

[ask questions]: #communication
[reach out to us]: #communication
[mozilla-central]: https://searchfox.org/mozilla-central/source/testing/marionette/
[good first bugs]: https://www.joshmatthews.net/bugsahoy/?automation=1&js=1
[Developer Guide]: https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide


Building
--------

As Marionette is built in to Firefox and ships with official Firefox
releases, it is included in a normal Firefox build.  To get your
development environment set up you can run this command on any
system and follow the on-screen instructions:

	% ./mach bootstrap

As Marionette is written in [XPCOM] flavoured JavaScript, you may
choose to rely on so called [artifact builds], which will download
pre-compiled Firefox blobs to your computer.  This means you don’t
have to compile Firefox locally, but does come at the cost of having
a good internet connection.  To enable [artifact builds] you may
add this line to the _mozconfig_ file in your top source directory:

	ac_add_options --enable-artifact-builds

To perform a regular build, simply do:

	% ./mach build

You can clean out the objdir using this command:

	% ./mach clobber

Occasionally a clean build will be required after you fetch the
latest changes from mozilla-central.  You will find that the the
build will error when this is the case.  To automatically do clean
builds when this happens you may optionally add this line to the
_mozconfig_ file in your top source directory:

	mk_add_options AUTOCLOBBER=1

If you compile Firefox frequently you will also want to enable
[ccache] and [sccache] if you develop on a macOS or Linux system:

	mk_add_options 'export RUSTC_WRAPPER=sccache'
	mk_add_options 'export CCACHE_CPP2=yes'
	ac_add_options --with-ccache

[artifact builds]: https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Build_Instructions/Artifact_builds
[ccache]: https://ccache.samba.org/
[sccache]: https://github.com/mozilla/sccache


Running tests
-------------

We verify and test Marionette in a couple of different ways.
Marionette has a set of [xpcshell] unit tests located in
_testing/marionette/test_*.js_.  These can be run this way:

	% ./mach xpcshell-test testing/marionette/test_error.js

Because tests are run in parallell and xpcshell itself is quite
chatty, it can sometimes be useful to run the tests sequentially:

	% ./mach xpcshell-test --sequential testing/marionette/test_error.js

These unit tests run as part of the _X_ jobs on Treeherder.

We also have a set of functional tests that make use of the Marionette
Python client.  These start a Firefox process and tests the Marionette
protocol input and output.  The following command will run all tests:

	% ./mach marionette test

But you can also run individual tests:

	% ./mach marionette test testing/marionette/harness/marionette_harness/tests/unit/test_navigation.py 

When working on Marionette code it is often useful to surface the
stdout from Firefox:

	% ./mach marionette test --gecko-log -

It is common to use this in conjunction with an option to increase
the Marionette log level:

	% ./mach marionette test --gecko-log - -vv

A single `-v` enables debug logging, and a double `-vv` enables
trace logging.

As these are functional integration tests and pop up Firefox windows
sporadically, a helpful tip is to surpress the window whilst you
are running them by using Firefox’ [headless mode]:

	% ./mach marionette test --headless

It is equivalent to setting the `MOZ_HEADLESS` output variable.
In addition to `MOZ_HEADLESS` there is also `MOZ_HEADLESS_WIDTH` and
`MOZ_HEADLESS_HEIGHT` for controlling the dimensions of the no-op
virtual display.  This is similar to using xvfb(1) which you may
know from the X windowing system, but has the additional benefit
of also working on macOS and Windows.

We have a separate page documenting how to write good Python tests in
[[doc/PythonTests.md]].  These tests will run as part of the _Mn_
job on Treeherder.

In addition to these two test types that specifically test the
Marionette protocol, Marionette is used as the backend for the
[geckodriver] WebDriver implementation.  It is served by a WPT test
suite which effectively tests conformance to the W3C specification.

This is a good try syntax to use when testing Marionette changes:

	-b do -p linux,linux64,macosx64,win64,android-api-16 -u marionette-e10s,marionette-headless-e10s,xpcshell,web-platform-tests,firefox-ui-functional-local-e10s,firefox-ui-functional-remote-e10s -t none

[xpcshell]: https://developer.mozilla.org/en-US/docs/Mozilla/QA/Writing_xpcshell-based_unit_tests
[headless mode]: https://developer.mozilla.org/en-US/Firefox/Headless_mode
[geckodriver]: ../geckodriver/README.md


Submitting patches
------------------

You can submit patches by uploading .diff files to Bugzilla or by
sending them to [MozReview].

Once you have contributed a couple of patches, we are happy to
sponsor you in [becoming a Mozilla committer].  When you have been
granted commit access level 1 you will have permission to use the
[Firefox CI] to trigger your own “try runs” to test your changes.

[MozReview]: http://mozilla-version-control-tools.readthedocs.io/en/latest/mozreview.html
[becoming a Mozilla committer]: https://www.mozilla.org/en-US/about/governance/policies/commit/


Communication
=============

The mailing list for Marionette discussion is
tools-marionette@lists.mozilla.org ([subscribe], [archive]).

If you prefer real-time chat, there is often someone in the #ateam IRC
channel on irc.mozilla.org.  Don’t ask if you can ask a question, just
ask, and please wait for an answer as we might not be in your timezone.

[subscribe]: https://lists.mozilla.org/listinfo/tools-marionette
[archive]: https://groups.google.com/group/mozilla.tools.marionette
