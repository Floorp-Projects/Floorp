.. Licensed under the Apache License: http://www.apache.org/licenses/LICENSE-2.0
.. For details: https://github.com/nedbat/coveragepy/blob/master/NOTICE.txt

.. _api_module:

coverage module
---------------

.. module:: coverage

The most important thing in the coverage module is the
:class:`coverage.Coverage` class, described in :ref:`api_coverage`, but there
are a few other things also.


.. data:: version_info

A tuple of five elements, similar to :data:`sys.version_info
<python:sys.version_info>`: *major*, *minor*, *micro*, *releaselevel*, and
*serial*.  All values except *releaselevel* are integers; the release level is
``'alpha'``, ``'beta'``, ``'candidate'``, or ``'final'``. Unlike
:data:`sys.version_info <python:sys.version_info>`, the elements are not
available by name.

.. data:: __version__

A string with the version of coverage.py, for example, ``"5.0b2"``.

.. autoclass:: CoverageException


Starting coverage.py automatically
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This function is used to start coverage measurement automatically when Python
starts.  See :ref:`subprocess` for details.

.. autofunction:: process_startup
