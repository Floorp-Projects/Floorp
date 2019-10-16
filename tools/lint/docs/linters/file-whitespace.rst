Trailing whitespaces
====================

This linter verifies if a file has unnecessary trailing whitespaces or Windows
carriage return.


Run Locally
-----------

This mozlint linter can be run using mach:

.. parsed-literal::

    $ mach lint --linter file-whitespace <file paths>


Configuration
-------------

This linter is enabled on most of the code base.

Autofix
-------

This linter provides a ``--fix`` option. The python script is doing the change itself.

Sources
-------

* `Configuration <https://searchfox.org/mozilla-central/source/tools/lint/file-whitespace.yml>`_
* `Source <https://searchfox.org/mozilla-central/source/tools/lint/file-whitespace/__init__.py>`_
