.. _mach_usage:

==========
User Guide
==========

Mach is the central entry point for most operations that can be performed in
mozilla-central.


Command Help
------------

To see an overview of all the available commands, run:

.. code-block:: shell

    $ ./mach help

For more detailed information on a specific command, run:

.. code-block:: shell

    $ ./mach help <command>

If a command has subcommands listed, you can see more details on the subcommand
by running:

.. code-block:: shell

    $ ./mach help <command> <subcommand>

Alternatively, you can pass ``-h/--help``. For example, all of the
following are valid:

.. code-block:: shell

    $ ./mach help try
    $ ./mach help try fuzzy
    $ ./mach try -h
    $ ./mach try fuzzy --help


Auto Completion
---------------

A `bash completion`_ script is bundled with mach, it can be used with either ``bash`` or ``zsh``.

Bash
~~~~

Add the following to your ``~/.bashrc``, ``~/.bash_profile`` or equivalent:

.. code-block:: shell

    source <srcdir>/python/mach/bash-completion.sh

.. tip::

    Windows users using the default shell bundled with mozilla-build should source the completion
    script from ``~/.bash_profile`` (it may need to be created first).

Zsh
~~~

Add this to your ``~/.zshrc`` or equivalent:

.. code-block:: shell

    autoload -U bashcompinit && bashcompinit
    source <srcdir>/python/mach/bash-completion.sh

The ``compinit`` function also needs to be loaded, but if using a framework (like ``oh-my-zsh``),
this will often be done for you. So if you see ``command not found: compdef``, you'll need to modify
the above instructions to:

.. code-block:: shell

    autoload -U compinit && compinit
    autoload -U bashcompinit && bashcompinit
    source <srcdir>/python/mach/bash-completion.sh

Don't forget to substitute ``<srcdir>`` with the path to your checkout.


User Settings
-------------

Some mach commands can read configuration from a ``machrc`` file. The default
location for this file is ``~/.mozbuild/machrc`` (you'll need to create it).
This can also be set to a different location by setting the ``MACHRC``
environment variable.

For a list of all the available settings, run:

.. code-block:: shell

    $ ./mach settings

The settings file follows the ``ini`` format, e.g:

.. code-block:: ini

    [alias]
    eslint = lint -l eslint

    [build]
    telemetry = true

    [try]
    default = fuzzy


.. _bash completion: https://searchfox.org/mozilla-central/source/python/mach/bash-completion.sh
