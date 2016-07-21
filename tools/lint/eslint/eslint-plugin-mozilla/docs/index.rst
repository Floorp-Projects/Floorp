.. _index:

=====================
Mozilla ESLint Plugin
=====================

``balanced-listeners`` checks that every addEventListener has a
removeEventListener (and does the same for on/off).

``components-imports`` adds the filename of imported files e.g.
``Cu.import("some/path/Blah.jsm")`` adds Blah to the global scope.

``import-globals-from`` When the "import-globals-from <path>" comment is found
in a file, then all globals from the file at <path> will be imported in the
current scope.

``import-headjs-globals`` imports globals from head.js and from any files that
should be imported by head.js (as far as we can correctly resolve the path).

``mark-test-function-used`` simply marks test (the test method) as used. This
avoids ESLint telling us that the function is never called.

``no-aArgs`` prevents using the hungarian notation in function arguments.

``no-cpows-in-tests`` checks if the file is a browser mochitest and,
if so, checks for possible CPOW usage.

``no-single-arg-cu-import`` rejects calls to "Cu.import" that do not supply a
second argument (meaning they add the exported properties into global scope).

``reject-importGlobalProperties`` rejects calls to
"Cu.importGlobalProperties".  Use of this function is undesirable in
some parts of the tree.

``reject-some-requires`` rejects some calls to ``require``, according
to a regexp passed in as an option.

``this-top-level-scope`` treats top-level assignments like
``this.mumble = value`` as declaring a global.

Note: These are string matches so we will miss situations where the parent
object is assigned to another variable e.g.::

   var b = gBrowser;
   b.content // Would not be detected as a CPOW.

``var-only-at-top-level`` marks all var declarations that are not at the top
level invalid.

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

.. toctree::
   :maxdepth: 1

   balanced-listeners
   components-imports
   import-globals-from
   import-headjs-globals
   mark-test-function-used
   no-aArgs
   no-cpows-in-tests
   reject-importGlobalProperties
   reject-some-requires
   this-top-level-scope
   var-only-at-top-level
