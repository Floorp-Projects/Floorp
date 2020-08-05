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


Configuring Try with Mercurial / Git
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The recommended way to push to try is via the ``mach try`` command. This requires the
``push-to-try`` extension which can be installed by running:

.. code-block:: shell

    $ mach vcs-setup

.. _Level 1 Commit Access: https://www.mozilla.org/en-US/about/governance/policies/commit/access-policy/
.. _Becoming a Mozilla Committer: https://www.mozilla.org/en-US/about/governance/policies/commit/
