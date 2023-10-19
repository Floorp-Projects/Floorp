# Reporting bugs

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

Because of the wide variety and different characteristics of clients
used with geckodriver, their stacktraces, logs, and code examples are
typically not very useful as they distract from the actual underlying
cause.  **For this reason, we cannot overstate the importance of
always providing the [trace-level log] from geckodriver.** Bugs
relating to a specific client should be filed with that project.

We welcome you to file issues in the [GitHub issue tracker] once you are
confident it has not already been reported.  The [ISSUE_TEMPLATE.md]
contains a helpful checklist for things we will want to know about
the affected system, reproduction steps, and logs.

geckodriver development follows a rolling release model as
we don’t release patches for older versions.  It is therefore
useful to use the tip-of-tree geckodriver binary, or failing this,
the latest release when verifying the problem.  geckodriver is only
compatible with the current release channel versions of Firefox, and
it consequently does not help to report bugs that affect outdated
and unsupported Firefoxen.  Please always try to verify the issue
in the latest Firefox Nightly before you file your bug.

Once we are satisfied the issue raised is of sufficiently actionable
character, we will continue with triaging it and file a bug where it
is appropriate.  Bugs specific to geckodriver will be filed in the
[`Testing :: geckodriver`] component in Bugzilla.

[mailing list]: index.rst/#communication
[trace-level log]: TraceLogs.md
[GitHub issue tracker]: https://github.com/mozilla/geckodriver/issues
[ISSUE_TEMPLATE.md]: https://raw.githubusercontent.com/mozilla/geckodriver/master/ISSUE_TEMPLATE.md
[`Testing :: geckodriver`]: https://bugzilla.mozilla.org/buglist.cgi?component=geckodriver
