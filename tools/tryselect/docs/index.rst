Pushing to Try
==============

"Pushing to Try" allows developers to build and test their changes on Mozilla's automation servers
without requiring their code to be reviewed and landed.

First, :doc:`ensure that you can push to Try <configuration>`.
Try knows how to run tasks that are defined in-tree,
such as ``build-linux64/opt`` (build Firefox for Linux). To manually select some tasks for
Try to process, run the following command:

.. code-block:: shell

    ./mach try fuzzy

After submitting your requested tasks, you'll be given a link to your "push" in Treeherder.
It may take a few minutes for your push to appear in Treeherder! Be patient, and it will automatically
update when Try begins processing your work.

Another very useful Try command is ``./mach try auto``, which will automatically select the tasks
that are mostly likely to be affected by your changes.
See the :doc:`selectors page <selectors/index>` to view all the other ways to select which tasks to push.

Resolving "<Try build> is damaged and can't be opened" error
------------------------------------------------------------

To run a try build on macOS, you need to get around Apple's restrictions on downloaded applications.

These restrictions differ based on your hardware: Apple Silicon machines (M1 etc.) are much stricter.

For Apple Silicon machines, you will need to download the target.dmg artifact from the
"repackage-macosx64-shippable/opt" job.
This is a universal build (i.e. it contains both x86_64 and arm64 code), and it is signed but not notarized.
You can trigger this job using ``./mach try fuzzy --full``.

On Intel Macs, you can run unsigned builds, once you get around the quarantining (see below),
so you can just download the "target.dmg" from a regular opt build.

Regardless of hardware, you need to make sure that there is no quarantining attribute on
the downloaded dmg file before you attempt to run it:
Apple automatically quarantines apps that are downloaded with a browser from an untrusted
location. This "quarantine status" can be cleared by doing ``xattr -c <Try build>`` after
downloading. You can avoid this "quarantine status" by downloading the build from the command
line instead, such as by using ``curl``:

.. code-block:: shell

    curl -L <artifact-url> -o <file-name>

.. _attach-job-review:

Adding Try jobs to a Phabricator patch
--------------------------------------

For every patch submitted for review in Phabricator, a new Try run is automatically created.
A link called ``Treeherder Jobs`` can be found in the ``Diff Detail`` section of the review in
Phabricator.

.. image:: img/phab-treeherder-link.png

This run is created for static analysis, linting and other tasks. Attaching new jobs to the run is
easy and doesn't require more actions from the developer.
Click on the down-arrow to access the actions menu, select the relevant jobs
and, click on ``Trigger X new jobs`` (located on the top of the job).

.. image:: img/add-new-jobs.png

Table of Contents
-----------------

.. toctree::
  :maxdepth: 2

  configuration
  selectors/index
  presets
  tasks


Indices and tables
------------------

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
