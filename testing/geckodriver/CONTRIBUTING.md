Contributing
============

We are delighted that you want to help improve geckodriver!
Mozilla’s WebDriver implementation consists of a few different
parts it can be useful to know about:

  * [_geckodriver_] provides the HTTP API described by the [WebDriver
    protocol] to communicate with Gecko-based browsers such as
    Firefox and Fennec.  It is a standalone executable written in
    Rust, and can be used with compatible W3C WebDriver clients.

  * [_Marionette_] is the Firefox remote protocol used by geckodriver
    to communicate with, instrument, and control Gecko.  It is
    built in to Firefox and written in [XPCOM] flavoured JavaScript.

  * [_webdriver_] is a Rust crate providing interfaces, traits
    and types, errors, type- and bounds checks, and JSON marshaling
    for correctly parsing and emitting the [WebDriver protocol].

By participating in this project, you agree to abide by the Mozilla
[Community Participation Guidelines].  Here are some guidelines
for contributing high-quality and actionable bugs and code.

[_geckodriver_]: ./README.md
[_Marionette_]: ../marionette/README.md
[_webdriver_]: ../webdriver/README.md
[WebDriver protocol]: https://w3c.github.io/webdriver/webdriver-spec.html#protocol
[XPCOM]: https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XPCOM/Guide
[Community Participation Guidelines]: https://www.mozilla.org/en-US/about/governance/policies/participation/


Reporting bugs
==============

When opening a new issue or commenting on existing issues, please
make sure discussions are related to concrete technical issues
with geckodriver or Marionette.  Questions or discussions are more
appropriate for the [mailing list].

For issue reports to be actionable, it must be clear exactly
what the observed and expected behaviours are, and how to set up
the state required to observe the erroneous behaviour.  The most
useful thing to provide is a minimal HTML document which permits
the problem to be reproduced along with a [trace-level log] from
geckodriver showing the exact wire protocol calls made.

Because of the wide variety and different charateristics of clients
used with geckodriver, their stacktraces, logs, and code examples are
typically not very useful as they distract from the actual underlying
cause.  **For this reason, we cannot overstate the importance of
always providing the [trace-level log] from geckodriver.** Bugs
relating to a specific client should be filed with that project.

We welcome you to file issues in the [GitHub issue tracker] once you are
confident it has not already been reported.  The [ISSUE_TEMPLATE.md]
contains a helpful checklist for things we will want to know about
the affected system, reproduction steps, and logs.

geckodriver development follows a rolling release model as we don’t
release patches for older versions.  It is therefore useful to use
the tip-of-tree geckodriver binary, or failing this, the latest
release when verifying the problem.  Similarly, as noted in the
[README], geckodriver is only compatible with the current release
channel versions of Firefox, and it consequently does not help
to report bugs that affect outdated and unsupported Firefoxen.
Please always try to verify the issue in the latest Firefox Nightly
before you file your bug.

Once we are satisfied the issue raised is of sufficiently actionable
character, we will continue with triaging it and file a bug where it
is appropriate.  Bugs specific to geckodriver will be filed in the
[`Testing :: geckodriver`] component in Bugzilla.

[mailing list]: #communication
[trace-level log]: doc/TraceLogs.md
[GitHub issue tracker]: https://github.com/mozilla/geckodriver/issues
[README]: ./README.md
[`Testing :: geckodriver`]: https://bugzilla.mozilla.org/buglist.cgi?component=geckodriver


Writing code
============

Because there are many moving parts involved remote controlling
a web browser, it can be challenging to a new contributor to know
where to start.  Please don’t hesitate to [ask questions]!

The canonical source code repository of geckodriver is now
[mozilla-central].  We continue to use the [GitHub issue tracker] as
a triage ground before actual, actionable bugs and tasks are filed
in the [`Testing :: geckodriver`] component on Bugzilla.  We also
have a curated set of [good first bugs] you may consider attempting first.

The purpose of this guide _is not_ to make sure you have a basic
development environment set up.  For that there is plentiful
documentation, such as the [Developer Guide] to get you rolling.
Once you do, we can get started working up your first patch!
Remember to [reach out to us] at any point if you have questions.

[ask questions]: #communication
[reach out to us]: #communication
[mozilla-central]: https://searchfox.org/mozilla-central/source/testing/geckodriver/
[good first bugs]: https://www.joshmatthews.net/bugsahoy/?automation=1&rust=1
[Developer Guide]: https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide


Building geckodriver
--------------------

geckodriver is written in [Rust], a systems programming language
from Mozilla.  Crucially, it relies on the [webdriver crate] to
provide the HTTPD and do most of the heavy lifting of marshalling the
[WebDriver protocol].  geckodriver translates WebDriver [commands],
[responses], and [errors] to the [Marionette protocol], and acts
as a proxy between WebDriver clients and [Marionette].

Whilst geckodriver lives in the same source repository as Firefox
and is built in the [Firefox CI], is _is not_ built if you build
Firefox locally.  To enable building of geckodriver locally, ensure
you put this in your [mozconfig]:

	ac_add_options --enable-geckodriver

When you have, you are ready to start off your first build:

	% ./mach build testing/geckodriver

The geckodriver executable will appear in `${objdir}/dist/bin/geckodriver`
alongside firefox-bin.  To run it you can use mach:

	% ./mach geckodriver -- --version
	 0:00.27 /home/ato/src/gecko/obj-x86_64-pc-linux-gnu/dist/bin/geckodriver --version --binary /home/ato/src/gecko/obj-x86_64-pc-linux-gnu/dist/bin/firefox
	geckodriver 0.19.0 (f3e939a81ee1169f9501ad96eb43bbf4bf4a1bde 2017-10-11)

[Rust]: https://www.rust-lang.org/
[webdriver crate]: ../webdriver/README.md
[commands]: https://docs.rs/webdriver/newest/webdriver/command/index.html
[responses]: https://docs.rs/webdriver/newest/webdriver/response/index.html
[errors]: https://docs.rs/webdriver/newest/webdriver/error/enum.ErrorStatus.html
[Marionette protocol]: https://developer.mozilla.org/en-US/docs/Mozilla/QA/Marionette/Protocol
[Marionette]: ../marionette/README.md
[Firefox CI]: https://treeherder.mozilla.org/
[mozconfig]: https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Build_Instructions/Configuring_Build_Options


Running the tests
-----------------

We verify and test geckodriver in a couple of different ways.
Since it is an implementation of the WebDriver web standard, we share
a set of conformance tests with other browser vendors through the
[Web Platform Tests] (WPT) initiative.  This lets us ensure web
compatibility between _different_ WebDriver implementations for
different browsers.

In addition to the WPT tests, geckodriver and libwebdriver has
unit tests.  At the moment there is no way to run Rust unit tests
through mach, although this is being worked on.  For the moment
you need to kick off a separate build using [cargo]:

	% cd testing/geckodriver
	   Compiling geckodriver v0.19.0 (file:///home/ato/src/gecko/testing/geckodriver)
	…
	test result: ok. 7 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out

Because the unit tests _are not_ currently run in the [Firefox CI],
hopefully you will find that they all pass. (-:

To run the more extensive WPT tests you can use mach, but first
make sure you have a build of Firefox:

	% ./mach build
	% ./mach wpt testing/web-platform/tests/webdriver

As these are functional integration tests and pop up Firefox windows
sporadically, a helpful tip is to surpress the window whilst you
are running them by using Firefox’ [headless mode]:

	% MOZ_HEADLESS=1 ./mach wpt testing/web-platform/tests/webdriver

In addition to the `MOZ_HEADLESS` output variable there is also
`MOZ_HEADLESS_WIDTH` and `MOZ_HEADLESS_HEIGHT` to control the
dimensions of the no-op virtual display.  This is similar to using
xvfb(1) which you may know from the X windowing system, but has
the additional benefit of also working on macOS and Windows.

As you get in to development of geckodriver and Marionette you will
increasingly grow to understand our love for [trace-level logs].
They provide us with the input—the HTTP requests—from the client
(in WPT’s case from the tests’ use of a custom WebDriver client),
the translation geckodriver makes to the [Marionette protocol],
the log output from Marionette, its responses back to geckodriver,
and finally the output—or the HTTP response—back to the client.

The [trace-level logs] can be surfaced by passing on the `-vv`
flag to geckodriver through WPT:

	% ./mach wpt --webdriver-arg=-vv testing/web-platform/tests/webdriver

[Web Platform Tests]: http://web-platform-tests.org/
[cargo]: http://doc.crates.io/guide.html
[headless mode]: https://developer.mozilla.org/en-US/Firefox/Headless_mode


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

The mailing list for geckodriver discussion is
tools-marionette@lists.mozilla.org ([subscribe], [archive]).

If you prefer real-time chat, there is often someone in the #ateam IRC
channel on irc.mozilla.org.  Don’t ask if you can ask a question, just
ask, and please wait for an answer as we might not be in your timezone.

[subscribe]: https://lists.mozilla.org/listinfo/tools-marionette
[archive]: https://groups.google.com/group/mozilla.tools.marionette
