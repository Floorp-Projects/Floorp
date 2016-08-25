ESLint
======

`ESLint`_ is a popular linter for JavaScript.

Run Locally
-----------

The mozlint integration of `ESLint`_ can be run using mach:

.. parsed-literal::

    $ mach lint --linter eslint <file paths>

Alternatively, omit the ``--linter eslint`` and run all configured linters, which will include
ESLint.


Configuration
-------------

The `ESLint`_ mozilla-central integration uses a blacklist to exclude certain directories from being
linted. This lives in ``topsrcdir/.eslintignore``. If you don't wish your directory to be linted, it
must be added here.

The global configuration file lives in ``topsrcdir/.eslintrc``. This global configuration can be
overridden by including an ``.eslintrc`` in the appropriate subdirectory. For an overview of the
supported configuration, see `ESLint's documentation`_.


ESLint Plugin Mozilla
---------------------

In addition to default ESLint rules, there are several Mozilla-specific rules that are defined in
the :doc:`Mozilla ESLint Plugin <eslint-plugin-mozilla>`.


.. _ESLint: http://eslint.org/
.. _ESLint's documentation: http://eslint.org/docs/user-guide/configuring


.. toctree::
   :hidden:

   eslint-plugin-mozilla
