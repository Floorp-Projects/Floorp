# Required Preferences for Fission

Fission (site isolation for Firefox) introduced some architectural changes that are incompatible with our CDP implementation. To keep using CDP for Firefox, make sure the following preferences are set in the profile before starting Firefox with `--remote-debugging-port`:

* `fission.bfcacheInParent` should be set to `false`.

* `fission.webContentIsolationStrategy` should be set to `0`.

Without those preferences, expect issues related to navigation in several domains (Page, Runtime, ...).

Third party tools relying on CDP such as Puppeteer ensure that those preferences are correctly set before starting Firefox.

The work to lift those restrictions is tracked in [Bug 1732263](https://bugzilla.mozilla.org/show_bug.cgi?id=1732263) and [Bug 1706353](https://bugzilla.mozilla.org/show_bug.cgi?id=1706353).
