.. _index:

=====================
Mozilla ESLint Plugin
=====================

``balanced-listeners`` checks that every addEventListener has a
removeEventListener (and does the same for on/off).

``components-imports`` adds the filename of imported files e.g.
``Cu.import("some/path/Blah.jsm")`` adds Blah to the global scope.

``import-headjs-globals`` imports globals from head.js and from any files that
should be imported by head.js (as far as we can correctly resolve the path).

``mark-test-function-used`` simply marks test (the test method) as used. This
avoids ESLint telling us that the function is never called.

``var-only-at-top-level`` Marks all var declarations that are not at the top
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
     "mozilla/import-headjs-globals": 1,
     "mozilla/mark-test-function-used": 1,
     "mozilla/var-only-at-top-level": 1,
   }

.. toctree::
   :maxdepth: 1

   balanced-listeners
   components-imports
   import-headjs-globals
   mark-test-function-used
   var-only-at-top-level
