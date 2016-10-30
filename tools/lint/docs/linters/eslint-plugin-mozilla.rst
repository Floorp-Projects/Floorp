=====================
Mozilla ESLint Plugin
=====================


balanced-listeners
------------------

Checks that for every occurence of 'addEventListener' or 'on' there is an
occurence of 'removeEventListener' or 'off' with the same event name.


components-imports
------------------

Checks the filename of imported files e.g. ``Cu.import("some/path/Blah.jsm")``
adds Blah to the global scope.


import-browserjs-globals
------------------------

When included files from the main browser UI scripts will be loaded and any
declared globals will be defined for the current file. This is mostly useful for
browser-chrome mochitests that call browser functions.


import-globals-from
-------------------

Parses a file for globals defined in various unique Mozilla ways.

When a "import-globals-from <path>" comment is found in a file, then all globals
from the file at <path> will be imported in the current scope. This will also
operate recursively.

This is useful for scripts that are loaded as <script> tag in a window and rely
on each other's globals.

If <path> is a relative path, then it must be relative to the file being
checked by the rule.


import-headjs-globals
---------------------

Import globals from head.js and from any files that were imported by
head.js (as far as we can correctly resolve the path).

The following file import patterns are supported:

-  ``Services.scriptloader.loadSubScript(path)``
-  ``loader.loadSubScript(path)``
-  ``loadSubScript(path)``
-  ``loadHelperScript(path)``
-  ``import-globals-from path``

If path does not exist because it is generated e.g.
``testdir + "/somefile.js"`` we do our best to resolve it.

The following patterns are supported:

-  ``Cu.import("resource://devtools/client/shared/widgets/ViewHelpers.jsm");``
-  ``loader.lazyImporter(this, "name1");``
-  ``loader.lazyRequireGetter(this, "name2"``
-  ``loader.lazyServiceGetter(this, "name3"``
-  ``XPCOMUtils.defineLazyModuleGetter(this, "setNamedTimeout", ...)``
-  ``loader.lazyGetter(this, "toolboxStrings"``
-  ``XPCOMUtils.defineLazyGetter(this, "clipboardHelper"``


mark-test-function-used
-----------------------

Simply marks `test` (the test method) or `run_test` as used when in mochitests
or xpcshell tests respectively. This avoids ESLint telling us that the function
is never called.


no-aArgs
--------

Checks that function argument names don't start with lowercase 'a' followed by
a capital letter. This is to prevent the use of Hungarian notation whereby the
first letter is a prefix that indicates the type or intended use of a variable.


no-cpows-in-tests
-----------------

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


no-single-arg-cu-import
-----------------------

Rejects calls to "Cu.import" that do not supply a second argument (meaning they
add the exported properties into global scope).


reject-importGlobalProperties
-----------------------------

Rejects calls to ``Cu.importGlobalProperties``.  Use of this function is
undesirable in some parts of the tree.


reject-some-requires
--------------------

This takes an option, a regular expression.  Invocations of
``require`` with a string literal argument are matched against this
regexp; and if it matches, the ``require`` use is flagged.


this-top-level-scope
--------------------

Treats top-level assignments like ``this.mumble = value`` as declaring a global.

Note: These are string matches so we will miss situations where the parent
object is assigned to another variable e.g.::

   var b = gBrowser;
   b.content // Would not be detected as a CPOW.


var-only-at-top-level
---------------------

Marks all var declarations that are not at the top level invalid.


Example
=======

+-------+-----------------------+
| Possible values for all rules |
+-------+-----------------------+
| Value | Meaning               |
+-------+-----------------------+
| 0     | Deactivated           |
+-------+-----------------------+
| 1     | Warning               |
+-------+-----------------------+
| 2     | Error                 |
+-------+-----------------------+

Example configuration::

   "rules": {
     "mozilla/balanced-listeners": 2,
     "mozilla/components-imports": 1,
     "mozilla/import-globals-from": 1,
     "mozilla/import-headjs-globals": 1,
     "mozilla/mark-test-function-used": 1,
     "mozilla/var-only-at-top-level": 1,
     "mozilla/no-cpows-in-tests": 1,
   }
