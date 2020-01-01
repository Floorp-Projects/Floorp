Mercurial
=========

Mercurial is a source-code management tool which allows users to keep track of changes to the source code locally and share their changes with others.
We use it for the development of Firefox.

Installation
------------

See `Mercurial Page <https://www.mercurial-scm.org/downloads>`__ for installation.

`More information <https://developer.mozilla.org/docs/Mozilla/Mercurial/Installing_Mercurial>`__

Basic configuration
-------------------

You should configure Mercurial before submitting patches to Mozilla.

If you will be pulling the Firefox source code or one of the derived repositories, the easiest way to configure Mercurial is to run the vcs-setup mach command:

.. code-block:: shell

    $ ./mach vcs-setup

This command starts an interactive wizard that will help ensure your Mercurial is configured with the latest recommended settings. This command will not change any files on your machine without your consent.

Extensions
----------

There's a number of extensions you can enable. See http://mercurial.selenic.com/wiki/UsingExtensions. Almost everyone should probably enable the following:

#. color - Colorize terminal output
#. histedit - Provides git rebase --interactive behavior.
#. progress - Draw progress bars on long-running operations.
#. rebase - Ability to easily rebase patches on top of other heads.

These can all be turned on by just adding this to your `.hgrc` file:

.. code-block:: shell

    [extensions]
    color =
    rebase =
    histedit =
    progress =

Configuring the try repository
------------------------------

If you have access to the try server you may want to configure Mercurial so you can refer to it simply as "try", since it can be useful from all your trees.  This can be done by adding this to your `~/.hgrc` file:

.. code-block:: shell

    [paths]
    try = ssh://hg.mozilla.org/try/

More documentation about mercurial
----------------------------------
https://developer.mozilla.org/docs/Mozilla/Developer_guide/Source_Code/Mercurial

https://developer.mozilla.org/docs/Mozilla/Mercurial

https://developer.mozilla.org/docs/Mozilla/Mercurial/Basics
