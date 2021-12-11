macOS notarization
==================

With the introduction of macOS 10.15 “Catalina” Apple introduced
[new notarization requirements] that all software must be signed
and notarized centrally.

Whilst geckodriver is technically both signed and notarized, the
way we package geckodriver on macOS means the notarization is lost.
Mozilla considers this a known bug with the [geckodriver 0.26.0
release] and are taking steps to resolve this.  You can track the
progress in [bug 1588081].

There are some mitigating circumstances:

  * Verification problems only occur when other notarized programs,
    such as a web browser, downloads the software from the internet.

  * Arbitrary software downloaded through other means, such as
    curl(1) is _not_ affected by this change.

In other words, if your method for fetching geckodriver on macOS
is through the GitHub web UI using a web browser, the program will
not be able to run unless you manually disable the quarantine check
(explained below).  If downloading geckodriver via other means
than a macOS notarized program, you should not be affected.

To bypass the notarization requirement on macOS if you have downloaded
the geckodriver .tar.gz via a web browser, you can run the following
command in a terminal:

	% xattr -r -d com.apple.quarantine geckodriver

A problem with notarization will manifest itself through a security
dialogue appearing, explaining that the source of the program is
not trusted.


[new notarization requirements]: https://developer.apple.com/news/?id=04102019a
[geckodriver 0.26.0 release]: https://github.com/mozilla/geckodriver/releases/tag/v0.26.0
[bug 1588081]: https://bugzilla.mozilla.org/show_bug.cgi?id=1588081
