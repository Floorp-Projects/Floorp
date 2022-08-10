.. -*- Mode: rst; fill-column: 80; -*-

=================================
Mozilla Central Contributor Guide
=================================

Table of contents
=================

.. contents:: :local:

Submitting a patch to Firefox using Git.
========================================

This guide will take you through submitting and updating a patch to
``mozilla-central`` as a git user. You need to already be `set up to use
git to contribute to mozilla-central <mc-quick-start.html>`_.

Performing a bug fix
--------------------

All of the open bugs for issues in Firefox can be found in
`Bugzilla <https://bugzilla.mozilla.org>`_. If you know the component
that you wish to contribute to you can use Bugzilla to search for issues
in that project. If you are unsure which component you are interested
in, you can search the `Good First
Bugs <https://bugzilla.mozilla.org/buglist.cgi?quicksearch=good-first-bug>`_
list to find something you want to work on.

-  Once you have your bug, assign it to yourself in Bugzilla.
-  Update your local copy of the firefox codebase to match the current
   version on the servers to ensure you are working with the most up to
   date code.

.. code:: bash

   git remote update

-  Create a new feature branch tracking either Central or Inbound.

.. code:: bash

   git checkout -b bugxxxxxxx [inbound|central]/default

-  Work on your bug, checking into git according to your preferred
   workflow. *Try to ensure that each individual commit compiles and
   passes all of the tests for your component. This will make it easier
   to land if you use ``moz-phab`` to submit (details later in this
   post).*

It may be helpful to have Mozilla commit access, at least level 1. There
are three levels of commit access that give increasing levels of access
to the repositories.

Level 1: Try/User access. You will need this level of access commit to
the try server.

Level 2: General access. This will give you full commit
access to any mercurial or SVN repository not requiring level 3 access.

Level 3: Core access. You will need this level to commit directly to any
of the core repositories (Firefox/Thunderbird/Fennec).

If you wish to apply for commit access, please follow the guide found in
the `Mozilla Commit Access
Policy <https://www.mozilla.org/en-US/about/governance/policies/commit/access-policy/>`_.

Submitting a patch that touches C/C++
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If your patch makes changes to any C or C++ code and your editor does
not have ``clang-format`` support, you should run the clang-format
linter before submitting your patch to ensure that your code is properly
formatted.

.. code:: bash

   mach clang-format -p path/to/file.cpp

Note that ``./mach bootstrap`` will offer to set up a commit hook that
will automatically do this for you.

Submitting to ``try`` with Level 1 commit access.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you only have Level 1 access, you will still need to submit your
patch through phabricator, but you can test it on the try server first.

-  Use ``./mach try fuzzy`` to select jobs to run and push to try.

Submitting a patch via Phabricator.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To commit anything to the repository, you will need to set up moz-phab
and Phabricator. If you are using ``git-cinnabar`` then you will need to
use git enabled versions of these tools.

Set up Phabricator
^^^^^^^^^^^^^^^^^^

-  In a browser, visit Mozilla’s Phabricator instance at
   https://phabricator.services.mozilla.com/.

-  Click “Log In” at the top of the page

   .. figure:: ../assets/LogInPhab.png
      :alt: Log in to Phabricator

      alt text

-  Click the “Log In or Register” button on the next page. This will
   take you to Bugzilla to log in or register a new account.

   .. figure:: ../assets/LogInOrRegister.png
      :alt: Log in or register a Phabiricator account

      alt text

-  Sign in with your Bugzilla credentials, or create a new account.

   .. figure:: ../assets/LogInBugzilla.png
      :alt: Log in with Bugzilla

      alt text

-  You will be redirected back to Phabricator, where you will have to
   create a new Phabricator account.

   .. raw:: html

      <Screenshot Needed>

-  Fill in/amend any fields on the form and click “Register Account”.

   .. raw:: html

      <Screenshot Needed>

-  You now have a Phabricator account and can submit and review patches.

Installing ``moz-phab``
^^^^^^^^^^^^^^^^^^^^^^^

.. code:: bash

   pip install MozPhab [--user]

Submitting a patch using ``moz-phab``.
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

-  Ensure you are on the branch where you have commits that you want to
   submit.

.. code:: bash

   git checkout your-branch

-  Check the revision numbers for the commits you want to submit

.. code:: bash

   git log

-  Run ``moz-phab``. Specifying a start commit will submit all commits
   from that commit. Specifying an end commit will submit all commits up
   to that commit. If no positional arguments are provided, the range is
   determined to be starting with the first non-public, non-obsolete
   changeset (for Mercurial) and ending with the currently checked-out
   changeset.

.. code:: bash

   moz-phab submit [start_rev] [end_rev]

-  You will receive a Phabricator link for each commit in the set.

Updating a patch
~~~~~~~~~~~~~~~~

-  Often you will need to make amendments to a patch after it has been
   submitted to address review comments. To do this, add your commits to
   the base branch of your fix as normal.

For ``moz-phab`` run in the same way as the initial submission with the
same arguments, that is, specifying the full original range of commits.
Note that, while inserting and amending commits should work fine,
reordering commits is not yet supported, and deleting commits will leave
the associated revisions open, which should be abandoned manually
