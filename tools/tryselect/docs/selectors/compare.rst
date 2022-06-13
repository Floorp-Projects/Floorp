Compare Selector
================

When this command runs it pushes two identical try jobs to treeherder. The first
job is on the current commit you are on, and the second one is a commit
specified in the command line arguments. This selector is aimed at helping
engineers test performance enhancements or resolve performance regressions.

Currently the only way you can select jobs is through fuzzy but we are
planning on expanding to other choosing frameworks also.

You pass the commit you want to compare against as a commit hash as either
``-cc`` or ``--compare-commit``, an example is show below

.. code-block:: shell

   $ mach try compare --compare-commit <commit-hash>
