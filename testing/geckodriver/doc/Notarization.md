# MacOS notarization

With the introduction of macOS 10.15 “Catalina” Apple introduced
[new notarization requirements] that all software must be signed
and notarized centrally.

Whilst the geckodriver binary is technically both signed and notarized, the
actual validation can only be performed by MacOS if the machine that starts
the geckodriver binary for the very first time is online. Offline validation
would require shipping geckodriver as a DMG/PKG. You can track the relevant
progress in [bug 1783943].

Note: geckodriver releases between 0.26.0 and 0.31.0 don't have the
notarization applied and always require the manual steps below to
bypass the notarization requirement of the binary during the very first start.

[new notarization requirements]: https://developer.apple.com/news/?id=04102019a
[bug 1783943]: https://bugzilla.mozilla.org/show_bug.cgi?id=1783943

## Offline mode

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
