File permission
===============

This linter verifies if a file has unnecessary permissions.
If a file has execution permissions (+x), file-perm will
generate a warning.

It will ignore files starting with ``#!`` (Python or node scripts).

This linter does not have any affect on Windows.


Run Locally
-----------

This mozlint linter can be run using mach:

.. parsed-literal::

    $ mach lint --linter file-perm <file paths>


Configuration
-------------

This linter is enabled on the whole code base.

Autofix
-------

This linter provides a ``--fix`` option. The python script is doing the change itself.

