.. _index:

=====================
Mozilla ESLint Plugin
=====================

``components-imports`` adds the filename of imported files e.g.
``Cu.import("some/path/Blah.jsm")`` adds Blah to the global scope.

``import-headjs-globals`` imports globals from head.js and from any files that
should be imported by head.js (as far as we can correctly resolve the path).

``mark-test-function-used`` simply marks test (the test method) as used. This
avoids ESLint telling us that the function is never called.

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
     "mozilla/components-imports": 1,
     "mozilla/import-headjs-globals": 1,
     "mozilla/mark-test-function-used": 1,
     "mozilla/var-only-at-top-level": 1,
   }

.. toctree::
   :maxdepth: 1

   components-imports
   import-headjs-globals
   mark-test-function-used
   var-only-at-top-level
