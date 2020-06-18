.. Licensed under the Apache License: http://www.apache.org/licenses/LICENSE-2.0
.. For details: https://github.com/nedbat/coveragepy/blob/master/NOTICE.txt

===========
Coverage.py
===========

Coverage.py is a tool for measuring code coverage of Python programs. It
monitors your program, noting which parts of the code have been executed, then
analyzes the source to identify code that could have been executed but was not.

Coverage measurement is typically used to gauge the effectiveness of tests. It
can show which parts of your code are being exercised by tests, and which are
not.

The latest version is coverage.py |release|, released |release_date|.  It is
supported on:

* Python versions 2.7, 3.5, 3.6, 3.7, 3.8, and 3.9 alpha.

* PyPy2 7.3.0 and PyPy3 7.3.0.

.. ifconfig:: prerelease

    **This is a pre-release build.  The usual warnings about possible bugs
    apply.** The latest stable version is coverage.py 4.5.4, `described here`_.

.. _described here: http://coverage.readthedocs.io/

For Enterprise
--------------

.. image:: media/Tidelift_Logos_RGB_Tidelift_Shorthand_On-White.png
   :width: 75
   :alt: Tidelift
   :align: left
   :class: tideliftlogo
   :target: https://tidelift.com/subscription/pkg/pypi-coverage?utm_source=pypi-coverage&utm_medium=referral&utm_campaign=readme

`Available as part of the Tidelift Subscription. <Tidelift Subscription_>`_ |br|
Coverage and thousands of other packages are working with
Tidelift to deliver one enterprise subscription that covers all of the open
source you use.  If you want the flexibility of open source and the confidence
of commercial-grade software, this is for you. `Learn more. <Tidelift
Subscription_>`_

.. _Tidelift Subscription: https://tidelift.com/subscription/pkg/pypi-coverage?utm_source=pypi-coverage&utm_medium=referral&utm_campaign=docs


Quick start
-----------

Getting started is easy:

#.  Install coverage.py::

        $ pip install coverage

    For more details, see :ref:`install`.

#.  Use ``coverage run`` to run your test suite and gather data. However you
    normally run your test suite, you can run your test runner under coverage.
    If your test runner command starts with "python", just replace the initial
    "python" with "coverage run".

    Instructions for specific test runners:

    .. tabs::

        .. tab:: pytest

            If you usually use::

                $ pytest arg1 arg2 arg3

            then you can run your tests under coverage with::

                $ coverage run -m pytest arg1 arg2 arg3

            Many people choose to use the `pytest-cov`_ plugin, but for most
            purposes, it is unnecessary.

        .. tab:: unittest

            Change "python" to "coverage run", so this::

                $ python -m unittest discover

            becomes::

                $ coverage run -m unittest discover

        .. tab:: nosetest

            *Nose has been unmaintained for a long time. You should seriously
            consider adopting a different test runner.*

            Change this::

                $ nosetests arg1 arg2

            to::

                $ coverage run -m nose arg1 arg2

    To limit coverage measurement to code in the current directory, and also
    find files that weren't executed at all, add the ``--source=.`` argument to
    your coverage command line.

#.  Use ``coverage report`` to report on the results::

        $ coverage report -m
        Name                      Stmts   Miss  Cover   Missing
        -------------------------------------------------------
        my_program.py                20      4    80%   33-35, 39
        my_other_module.py           56      6    89%   17-23
        -------------------------------------------------------
        TOTAL                        76     10    87%

#.  For a nicer presentation, use ``coverage html`` to get annotated HTML
    listings detailing missed lines::

        $ coverage html

    .. ifconfig:: not prerelease

        Then open htmlcov/index.html in your browser, to see a
        `report like this`_.

    .. ifconfig:: prerelease

        Then open htmlcov/index.html in your browser, to see a
        `report like this one`_.


.. _report like this: https://nedbatchelder.com/files/sample_coverage_html/index.html
.. _report like this one: https://nedbatchelder.com/files/sample_coverage_html_beta/index.html


Using coverage.py
-----------------

There are a few different ways to use coverage.py.  The simplest is the
:ref:`command line <cmd>`, which lets you run your program and see the results.
If you need more control over how your project is measured, you can use the
:ref:`API <api>`.

Some test runners provide coverage integration to make it easy to use
coverage.py while running tests.  For example, `pytest`_ has the `pytest-cov`_
plugin.

You can fine-tune coverage.py's view of your code by directing it to ignore
parts that you know aren't interesting.  See :ref:`source` and :ref:`excluding`
for details.

.. _pytest: http://doc.pytest.org
.. _pytest-cov: https://pytest-cov.readthedocs.io/


.. _contact:

Getting help
------------

If the :ref:`FAQ <faq>` doesn't answer your question, you can discuss
coverage.py or get help using it on the `Testing In Python`_ mailing list.

.. _Testing In Python: http://lists.idyll.org/listinfo/testing-in-python

Bug reports are gladly accepted at the `GitHub issue tracker`_.
GitHub also hosts the `code repository`_.

.. _GitHub issue tracker: https://github.com/nedbat/coveragepy/issues
.. _code repository: https://github.com/nedbat/coveragepy

Professional support for coverage.py is available as part of the `Tidelift
Subscription`_.

`I can be reached`_ in a number of ways. I'm happy to answer questions about
using coverage.py.

.. _I can be reached: https://nedbatchelder.com/site/aboutned.html



More information
----------------

.. toctree::
    :maxdepth: 1

    install
    For enterprise <https://tidelift.com/subscription/pkg/pypi-coverage?utm_source=pypi-coverage&utm_medium=referral&utm_campaign=enterprise>
    cmd
    config
    source
    excluding
    branch
    subprocess
    contexts
    api
    howitworks
    plugins
    contributing
    trouble
    faq
    whatsnew5x
    changes
    sleepy
