.. _import-globals:

==============
import-globals
==============

Rule Details
------------

Parses a file for globals defined in various unique Mozilla ways.

When a "import-globals-from <path>" comment is found in a file, then all globals
from the file at <path> will be imported in the current scope. This will also
operate recursively.

This is useful for scripts that are loaded as <script> tag in a window and rely
on each other's globals.

If <path> is a relative path, then it must be relative to the file being
checked by the rule.
