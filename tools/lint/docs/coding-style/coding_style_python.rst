===================
Python Coding style
===================

Coding style
~~~~~~~~~~~~

`black <https://github.com/psf/black/>`_ is the tool used to reformat the Python code.

Linting
~~~~~~~

The Python linting is done by `flake8 <https://gitlab.com/pycqa/flake8>`_.
flake8 is executed by mozlint both at review phase and in the CI.
See the :ref:`flake8` documentation for more information.


Indentation
~~~~~~~~~~~

Four spaces in Python code.


Makefile/moz.build practices
----------------------------

-  Changes to makefile and moz.build variables do not require
   build-config peer review. Any other build system changes, such as
   adding new scripts or rules, require review from the build-config
   team.
-  Suffix long ``if``/``endif`` conditionals with #{ & #}, so editors
   can display matched tokens enclosing a block of statements.

   ::

      ifdef CHECK_TYPE #{
        ifneq ($(flavor var_type),recursive) #{
          $(warning var should be expandable but detected var_type=$(flavor var_type))
        endif #}
      endif #}

-  moz.build are python and follow normal Python style.
-  List assignments should be written with one element per line. Align
   closing square brace with start of variable assignment. If ordering
   is not important, variables should be in alphabetical order.

   .. code-block:: python

      var += [
          'foo',
          'bar'
      ]

-  Use ``CONFIG['CPU_ARCH'] {=arm}`` to test for generic classes of
   architecture rather than ``CONFIG['OS_TEST'] {=armv7}`` (re: bug 886689).


Other advices
~~~~~~~~~~~~~

-  Install the
   `mozext <https://hg.mozilla.org/hgcustom/version-control-tools/file/default/hgext/mozext>`__
   Mercurial extension, and address every issue reported on commit,
   qrefresh, or the output of ``hg critic``.
-  Follow `PEP 8 <https://www.python.org/dev/peps/pep-0008/>`__.
-  Do not place statements on the same line as ``if/elif/else``
   conditionals to form a one-liner.
-  Global vars, please avoid them at all cost.
-  Exclude outer parenthesis from conditionals.Use
   ``if x > 5:,``\ rather than ``if (x > 5):``
-  Use string formatters, rather than var + str(val).
   ``var = 'Type %s value is %d'% ('int', 5).``
-  Utilize tools like
   `pylint <https://pypi.python.org/pypi/pylint>`__ or
   `pychecker <http://pychecker.sourceforge.net>`__ to screen
   sources for common problems.
-  Testing/Unit tests, please write them.
