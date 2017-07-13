=====================
Mozilla ESLint Plugin
=====================

Environments
============

These environments are available by specifying a comment at the top of the file,
e.g.

   /* eslint-env mozilla/chrome-worker */

There are also built-in ESLint environments available as well:
http://eslint.org/docs/user-guide/configuring#specifying-environments

browser-window
--------------

Defines the environment for scripts that are in the main browser.xul scope (
note: includes the places-overlay environment).

places-overlay
--------------

Defines the environment for scripts that are in a scope where placesOverlay.xul
is included.

chrome-worker
-------------

Defines the environment for chrome workers. This differs from normal workers by
the fact that `ctypes` can be accessed as well.

frame-script
------------

Defines the environment for frame scripts.

Rules
=====

avoid-removeChild
-----------------

Rejects using element.parentNode.removeChild(element) when element.remove()
can be used instead.

avoid-nsISupportsString-preferences
-----------------------------------

Rejects using getComplexValue and setComplexValue with nsISupportsString.

balanced-listeners
------------------

Checks that for every occurence of 'addEventListener' or 'on' there is an
occurence of 'removeEventListener' or 'off' with the same event name.

import-browser-window-globals
-----------------------------

For scripts included in browser-window, this will automatically inject the
browser-window global scopes into the file.

import-content-task-globals
---------------------------

For files containing ContentTask.spawn calls, this will automatically declare
the frame script variables in the global scope. ContentTask is only available
to test files, so by default the configs only specify it for the mochitest based
configurations.

This saves setting the file as a mozilla/frame-script environment.

Note: due to the way ESLint works, it appears it is only easy to declare these
variables on a file global scope, rather than function global. This may mean that
they are incorrectly allowed, but given they are test files, this should be
detected during testing.

import-globals
--------------

Checks the filename of imported files e.g. ``Cu.import("some/path/Blah.jsm")``
adds Blah to the global scope.

Note: uses modules.json for a list of globals listed in each file.


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


no-import-into-var-and-global
-----------------------------

Reject use of ``Cu.import`` (or ``Components.utils.import``) where it attempts to
import into a var and into the global scope at the same time, e.g.

``var foo = Cu.import("path.jsm", this);``

This is considered bad practice as it is confusing as to what is actually being
imported.

no-useless-parameters
---------------------

Reject common XPCOM methods called with useless optional parameters (eg.
``Services.io.newURI(url, null, null)``, or non-existent parameters (eg.
``Services.obs.removeObserver(name, observer, false)``).

This option can be autofixed (``--fix``).

no-useless-removeEventListener
------------------------------

Reject calls to removeEventListener where {once: true} could be used instead.

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

use-default-preference-values
---------------

Require providing a second parameter to get*Pref methods instead of
using a try/catch block.

use-ownerGlobal
---------------

Require .ownerGlobal instead of .ownerDocument.defaultView.


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
     "mozilla/mark-test-function-used": 1,
     "mozilla/var-only-at-top-level": 1,
     "mozilla/no-cpows-in-tests": 1,
   }

Tests
=====

The tests for eslint-plugin-mozilla are run via `mochajs`_ on top of node. Most
of the tests use the `ESLint Rule Unit Test framework`_.

Running Tests
-------------

The rules have some self tests, these can be run via:

   cd tools/lint/eslint/eslint-plugin-mozilla
   npm install
   npm run test

.. _mochajs: https://mochajs.org/
.. _ESLint Rule Unit Test Framework: http://eslint.org/docs/developer-guide/working-with-rules#rule-unit-tests

Disabling tests
---------------

In the unlikely event of needing to disable a test, currently the only way is
by commenting-out. Please file a bug if you have to do this.

Filing Bugs
===========

Bugs should be filed in the Testing product under Lint.
