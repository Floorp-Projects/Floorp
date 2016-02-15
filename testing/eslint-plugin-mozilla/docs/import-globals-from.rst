.. _import-globals-from:

===================
import-globals-from
===================

Rule Details
------------

When the "import-globals-from <path>" comment is found in a file, then all
globals from the file at <path> will be imported in the current scope.

This is useful for tests that rely on globals defined in head.js files, or for
scripts that are loaded as <script> tag in a window in rely on eachother's
globals.

If <path> is a relative path, then it must be relative to the file being
checked by the rule.
