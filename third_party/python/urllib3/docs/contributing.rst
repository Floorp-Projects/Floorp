Contributing
============

urllib3 is a community-maintained project and we happily accept contributions.

If you wish to add a new feature or fix a bug:

#. `Check for open issues <https://github.com/urllib3/urllib3/issues>`_ or open
   a fresh issue to start a discussion around a feature idea or a bug. There is
   a *Contributor Friendly* tag for issues that should be ideal for people who
   are not very familiar with the codebase yet.
#. Fork the `urllib3 repository on Github <https://github.com/urllib3/urllib3>`_
   to start making your changes.
#. Write a test which shows that the bug was fixed or that the feature works
   as expected.
#. Format your changes with black using command `$ nox -s blacken` and lint your
   changes using command `nox -s lint`.
#. Send a pull request and bug the maintainer until it gets merged and published.
   :) Make sure to add yourself to ``CONTRIBUTORS.txt``.


Setting up your development environment
---------------------------------------

In order to setup the development environment all that you need is 
`nox <https://nox.thea.codes/en/stable/index.html>`_ installed in your machine::

  $ pip install --user --upgrade nox


Running the tests
-----------------

We use some external dependencies, multiple interpreters and code coverage
analysis while running test suite. Our ``noxfile.py`` handles much of this for
you::

  $ nox --sessions test-2.7 test-3.7
  [ Nox will create virtualenv, install the specified dependencies, and run the commands in order.]
  nox > Running session test-2.7
  .......
  .......
  nox > Session test-2.7 was successful.
  .......
  .......
  nox > Running session test-3.7
  .......
  .......
  nox > Session test-3.7 was successful.

There is also a nox command for running all of our tests and multiple python
versions.

  $ nox --sessions test

Note that code coverage less than 100% is regarded as a failing run. Some
platform-specific tests are skipped unless run in that platform.  To make sure
the code works in all of urllib3's supported platforms, you can run our ``tox``
suite::

  $ nox --sessions test
  [ Nox will create virtualenv, install the specified dependencies, and run the commands in order.]
  .......
  .......
  nox > Session test-2.7 was successful.
  nox > Session test-3.4 was successful.
  nox > Session test-3.5 was successful.
  nox > Session test-3.6 was successful.
  nox > Session test-3.7 was successful.
  nox > Session test-3.8 was successful.
  nox > Session test-pypy was successful.

Our test suite `runs continuously on Travis CI
<https://travis-ci.org/urllib3/urllib3>`_ with every pull request.

Releases
--------

A release candidate can be created by any contributor by creating a branch
named ``release-x.x`` where ``x.x`` is the version of the proposed release.

- Update ``CHANGES.rst`` and ``urllib3/__init__.py`` with the proper version number
  and commit the changes to ``release-x.x``.
- Open a pull request to merge the ``release-x.x`` branch into the ``master`` branch.
- Integration tests are run against the release candidate on Travis. From here on all
  the steps below will be handled by a maintainer so unless you receive review comments
  you are done here.
- Once the pull request is squash merged into master the merging maintainer
  will tag the merge commit with the version number:

  - ``git tag -a 1.24.1 [commit sha]``
  - ``git push origin master --tags``

- After the commit is tagged Travis will build the tagged commit and upload the sdist and wheel
  to PyPI and create a draft release on GitHub for the tag. The merging maintainer will
  ensure that the PyPI sdist and wheel are properly uploaded.
- The merging maintainer will mark the draft release on GitHub as an approved release.

Sponsorship
-----------

.. |tideliftlogo| image:: https://nedbatchelder.com/pix/Tidelift_Logos_RGB_Tidelift_Shorthand_On-White_small.png
   :width: 75
   :alt: Tidelift

.. list-table::
   :widths: 10 100

   * - |tideliftlogo|
     - Professional support for urllib3 is available as part of the `Tidelift
       Subscription`_.  Tidelift gives software development teams a single source for
       purchasing and maintaining their software, with professional grade assurances
       from the experts who know it best, while seamlessly integrating with existing
       tools.

.. _Tidelift Subscription: https://tidelift.com/subscription/pkg/pypi-urllib3?utm_source=pypi-urllib3&utm_medium=referral&utm_campaign=docs

Please consider sponsoring urllib3 development, especially if your company
benefits from this library.

Your contribution will go towards adding new features to urllib3 and making
sure all functionality continues to meet our high quality standards.

We also welcome sponsorship in the form of time. We greatly appreciate companies
who encourage employees to contribute on an ongoing basis during their work hours.
Please let us know and we'll be glad to add you to our sponsors list!

A grant for contiguous full-time development has the biggest impact for
progress. Periods of 3 to 10 days allow a contributor to tackle substantial
complex issues which are otherwise left to linger until somebody can't afford
to not fix them.

Contact `@theacodes <https://github.com/theacodes>`_ or `@shazow <https://github.com/shazow>`_ 
to arrange a grant for a core contributor.

Huge thanks to all the companies and individuals who financially contributed to
the development of urllib3. Please send a PR if you've donated and would like
to be listed.

* `GOVCERT.LU <https://govcert.lu/>`_ (October 23, 2018)

* `Stripe <https://stripe.com/>`_ (June 23, 2014)

.. * [Company] ([date])
