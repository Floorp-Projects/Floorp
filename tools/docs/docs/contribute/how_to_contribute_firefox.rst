How to contribute to Firefox
============================

The goal of this doc is to have a place where all simple commands
are listed from start to end on a Linux/Mac system.

This aims to be a simple tutorial for lazy copy and paste.

Each section in this tutorial links to more detailed documentation on the topic.

Clone the sources
-----------------

We use mercurial, to clone the source:

.. code-block:: shell

    $ hg clone https://hg.mozilla.org/mozilla-central/

The clone should be around 30 minutes (depending on your connection) and
the repository should be less than 5GB (~ 20GB after the build).

`More
information <https://developer.mozilla.org/docs/Mozilla/Developer_guide/Source_Code/Mercurial>`__

Install dependencies
--------------------

Firefox provides a mechanism to install all dependencies; in the source tree:

.. code-block:: shell

     $ ./mach bootstrap

The default options are recommended.
Select "Artifact Mode" if you are not planning to write C++ or Rust code.

`More
information <https://developer.mozilla.org/docs/Mozilla/Developer_guide/Build_Instructions/Linux_Prerequisites>`__


To build & run
--------------

Once all the dependencies have been installed, run:

.. code-block:: shell

     $ ./mach build

which will check for dependencies and start the build.
This will take a while; a few minutes to a few hours depending on your hardware.

To run it:

.. code-block:: shell

     $ ./mach run

`More
information <https://developer.mozilla.org/docs/Mozilla/Developer_guide/Build_Instructions/Simple_Firefox_build/Linux_and_MacOS_build_preparation>`__


To write a patch
----------------

Make the changes you need in the code base.

Then:

.. code-block:: shell

    $ hg commit -m “Bug xxxx - the description of your change r?reviewer”

To find a reviewer, the easiest way is to do an “hg log” on the modified
file and look who usually is reviewing the actual changes (ie not
reformat, renaming of variables, etc).

To visualize your patch in the repository, run:

.. code-block:: shell

    $ hg wip

`More information <https://developer.mozilla.org/docs/Mozilla/Mercurial>`__


To test a change locally
------------------------

To run the tests, use mach test with the path. However, it isn’t
always easy to parse the results.

.. code-block:: shell

    $ ./mach test dom/serviceworkers

`More information <https://developer.mozilla.org/docs/Mozilla/QA/Automated_testing>`__

To test a change remotely
-------------------------

Running all the tests for Firefox takes a very long time and requires multiple
operating systems with various configurations. To build Firefox and run its
tests on continuous integration servers (CI), two commands are available:

.. code-block:: shell

    $ ./mach try chooser

To select jobs running a fuzzy search:

.. code-block:: shell

    $ ./mach try fuzzy

Note that it requires `level 1 permissions <https://www.mozilla.org/about/governance/policies/commit/access-policy/>`__.

`More information <https://firefox-source-docs.mozilla.org/tools/try/index.html>`__


To submit a patch
-----------------

To submit a patch for review, we use a tool called `moz-phab <https://moz-conduit.readthedocs.io/en/latest/phabricator-user.html#using-moz-phab>`__.

.. code-block:: shell

     $ moz-phab

It will publish all the currently applied patches to Phabricator and inform the reviewer.

If you wrote several patches on top of each other:

.. code-block:: shell

    $ moz-phab submit <first_revision>::<last_revision>

`More
information <https://moz-conduit.readthedocs.io/en/latest/phabricator-user.html>`__

To update a submitted patch
---------------------------

It is rare that a reviewer will accept the first version of patch. Moreover,
as the code review bot might suggest some improvements, changes to your patch
may be required.

Run:

.. code-block:: shell

    $ hg commit --amend <the modified file>

If you wrote many changes, you can squash or edit commits with the
command:

.. code-block:: shell

    $ hg histedit

(similar to `git rebase -i`)

The submission is the same as a the initial patch.


Retrieve new changes from the repository
----------------------------------------

To pull changes from the repository, run:

.. code-block:: shell

    $ hg update

If needed, to rebase a patch, run:

.. code-block:: shell

    $ hg rebase -s <origin_revision> -d <destination_revision>


To push a change in the code base
---------------------------------

Once the change has been accepted, ask the reviewer if they could land
the change. They don’t have an easy way to know if a contributor has
permission to land it or not.

If the reviewer does not land the patch after a few days, add
the *Check-in Needed* Tags to the review (*Edit Revision*).

The landing procedure will automatically close the review and the bug.

`More
information <https://developer.mozilla.org/docs/Mozilla/Developer_guide/How_to_Submit_a_Patch#Submitting_the_patch>`__

More documentation about contribution
-------------------------------------

https://developer.mozilla.org/docs/Mozilla/Developer_guide/Introduction

https://mozilla-version-control-tools.readthedocs.io/en/latest/devguide/contributing.html

https://moz-conduit.readthedocs.io/en/latest/phabricator-user.html
