.. Licensed under the Apache License: http://www.apache.org/licenses/LICENSE-2.0
.. For details: https://github.com/nedbat/coveragepy/blob/master/NOTICE.txt

.. _api:

===============
Coverage.py API
===============

There are a few different ways to use coverage.py programmatically.

The API to coverage.py is in a module called `coverage`.
Most of the interface is in the :class:`coverage.Coverage` class.  Methods on
the Coverage object correspond roughly to operations available in the command
line interface. For example, a simple use would be::

    import coverage

    cov = coverage.Coverage()
    cov.start()

    # .. call your code ..

    cov.stop()
    cov.save()

    cov.html_report()

Coverage.py supports plugins that can change its behavior, to collect
information from non-Python files, or to perform complex configuration.  See
:ref:`api_plugin` for details.

If you want to access the data that coverage.py has collected, the
:class:`coverage.CoverageData` class provides an API to read coverage.py data
files.

.. Note::
    Only the documented portions of the API are supported. Other names you may
    find in modules or objects can change their behavior at any time. Please
    limit yourself to documented methods to avoid problems.

For more intensive data use, you might want to access the coverage.py database
file directly.  The schema is subject to change, so this is for advanced uses
only.  :ref:`dbschema` explains more.

.. toctree::
    :maxdepth: 1

    api_coverage
    api_module
    api_plugin
    api_coveragedata
    dbschema
