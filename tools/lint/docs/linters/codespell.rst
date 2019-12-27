Codespell
=========

`codespell`_ is a popular tool to look for typical typos in the source code.

It is enabled mostly for the documentation and English locale files.

Run Locally
-----------

The mozlint integration of codespell can be run using mach:

.. parsed-literal::

    $ mach lint --linter codespell <file paths>


Configuration
-------------

To enable codespell on new directory, add the path to the include
section in the `codespell.yml <https://searchfox.org/mozilla-central/source/tools/lint/codespell.yml>`_ file.


Autofix
-------

Codespell provides a ``--fix`` option. It is based on the ``-w`` option provided by upstream.

.. _codespell: https://github.com/codespell-project/codespell/

Sources
-------

* `Configuration (YAML) <https://searchfox.org/mozilla-central/source/tools/lint/codespell.yml>`_
* `Source <https://searchfox.org/mozilla-central/source/tools/lint/spell/__init__.py>`_
