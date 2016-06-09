.. _no-cpows-in-tests:

=================
no-cpows-in-tests
=================

Rule Details
------------

This rule checks if the file is a browser mochitest and, if so, checks for
possible CPOW usage by checking for the following strings:

- "gBrowser.contentWindow"
- "gBrowser.contentDocument"
- "gBrowser.selectedBrowser.contentWindow"
- "browser.contentDocument"
- "window.content"
- "content"
- "content."

Note: These are string matches so we will miss situations where the parent
object is assigned to another variable e.g.::

   var b = gBrowser;
   b.content // Would not be detected as a CPOW.
