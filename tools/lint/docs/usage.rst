Running Linters Locally
=======================

Using the Command Line
----------------------

You can run all the various linters in the tree using the ``mach lint`` command. Simply pass in the
directory or file you wish to lint (defaults to current working directory):

.. parsed-literal::

    ./mach lint path/to/files

Multiple paths are allowed:

.. parsed-literal::

    ./mach lint path/to/foo.js path/to/bar.py path/to/dir

``Mozlint`` will automatically determine which types of files exist, and which linters need to be run
against them. For example, if the directory contains both JavaScript and Python files then mozlint
will automatically run both ESLint and Flake8 against those files respectively.

To restrict which linters are invoked manually, pass in ``-l/--linter``:

.. parsed-literal::

    ./mach lint -l eslint path/to/files

Finally, ``mozlint`` can lint the files touched by outgoing revisions or the working directory using
the ``-o/--outgoing`` and ``-w/--workdir`` arguments respectively. These work both with mercurial and
git. In the case of ``--outgoing``, the default remote repository the changes would be pushed to is
used as the comparison. If desired, a remote can be specified manually. In git, you may only want to
lint staged commits from the working directory, this can be accomplished with ``--workdir=staged``.
Examples:

.. parsed-literal::

    ./mach lint --workdir
    ./mach lint --workdir=staged
    ./mach lint --outgoing
    ./mach lint --outgoing origin/master
    ./mach lint -wo


Using a VCS Hook
----------------

There are also both pre-commit and pre-push version control hooks that work in
either hg or git. To enable a pre-push hg hook, add the following to hgrc:

.. parsed-literal::

    [hooks]
    pre-push.lint = python:/path/to/gecko/tools/lint/hooks.py:hg


To enable a pre-commit hg hook, add the following to hgrc:

.. parsed-literal::

    [hooks]
    pretxncommit.lint = python:/path/to/gecko/tools/lint/hooks.py:hg


To enable a pre-push git hook, run the following command:

.. parsed-literal::

    $ ln -s /path/to/gecko/tools/lint/hooks.py .git/hooks/pre-push


To enable a pre-commit git hook, run the following command:

.. parsed-literal::

    $ ln -s /path/to/gecko/tools/lint/hooks.py .git/hooks/pre-commit
