Configuring Try
===============


Getting Level 1 Commit Access
-----------------------------

In order to push to try, `Level 1 Commit Access`_ is required. Please see `Becoming a Mozilla
Committer`_ for more information on how to obtain this.


Configuring Version Control
---------------------------

After you have level 1 access, you'll need to do a little bit of setup before you can push. Both
``hg`` and ``git`` are supported, move on to the appropriate section below.


Configuring Try with Mercurial
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The recommended way to push to try is via the ``mach try`` command. This requires the
``push-to-try`` extension which can be installed by running:

.. code-block:: shell

    $ mach mercurial-setup

You should also enable the ``firefoxtree`` extension which will provide a handy ``try`` path alias.
You can also create this alias manually by adding

.. code-block:: ini

    [paths] try = ssh://hg.mozilla.org/try

This is only necessary if not using ``firefoxtree``.


Configuring Try with Git Cinnabar
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The recommended way to use try with git is with `git cinnabar`_. You can follow `this tutorial`_ for
a workflow which includes setting up the ability to push to try.


Configuring Try with Vanilla Git
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This workflow isn't well supported, but is possible using `moz-git-tools`_, and specifically the
`git push-to-try`_ command.


.. _Level 1 Commit Access: https://www.mozilla.org/en-US/about/governance/policies/commit/access-policy/
.. _Becoming a Mozilla Committer: https://www.mozilla.org/en-US/about/governance/policies/commit/
.. _git cinnabar: https://github.com/glandium/git-cinnabar/
.. _this tutorial: https://github.com/glandium/git-cinnabar/wiki/Mozilla:-A-git-workflow-for-Gecko-development
.. _moz-git-tools: https://github.com/mozilla/moz-git-tools
.. _git push-to-try: https://github.com/mozilla/moz-git-tools#git-push-to-try
