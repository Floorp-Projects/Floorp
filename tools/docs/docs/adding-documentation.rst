Adding Documentation
--------------------

To add new documentation, define the ``SPHINX_TREES`` and
``SPHINX_PYTHON_PACKAGE_DIRS`` variables in ``moz.build`` files in
the tree and documentation will automatically get picked up.

Say you have a directory ``featureX`` you would like to write some
documentation for. Here are the steps to create Sphinx documentation
for it:

1. Create a directory for the docs. This is typically ``docs``. e.g.
   ``featureX/docs``.
2. Create an ``index.rst`` file in this directory. The ``index.rst`` file
   is the root documentation for that section. See ``build/docs/index.rst``
   for an example file.
3. In a ``moz.build`` file (typically the one in the parent directory of
   the ``docs`` directory), define ``SPHINX_TREES`` to hook up the plumbing.
   e.g. ``SPHINX_TREES['featureX'] = 'docs'``. This says *the ``docs``
   directory under the current directory should be installed into the
   Sphinx documentation tree under ``/featureX``*.
4. If you have Python packages you would like to generate Python API
   documentation for, you can use ``SPHINX_PYTHON_PACKAGE_DIRS`` to
   declare directories containing Python packages. e.g.
   ``SPHINX_PYTHON_PACKAGE_DIRS += ['mozpackage']``.
5. In ``docs/config.yml``, defines in which category the doc
   should go.
6. Verify the rst syntax using `./mach lint -l rst`_

.. _./mach lint -l rst: /tools/lint/linters/rstlinter.html
