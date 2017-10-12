Reference Guide
===============

``virtualenv`` Command
----------------------

.. _usage:

Usage
~~~~~

:command:`virtualenv [OPTIONS] ENV_DIR`

    Where ``ENV_DIR`` is an absolute or relative path to a directory to create
    the virtual environment in.

.. _options:

Options
~~~~~~~

.. program: virtualenv

.. option:: --version

   show program's version number and exit

.. option:: -h, --help

   show this help message and exit

.. option:: -v, --verbose

   Increase verbosity.

.. option:: -q, --quiet

   Decrease verbosity.

.. option:: -p PYTHON_EXE, --python=PYTHON_EXE

   The Python interpreter to use, e.g.,
   --python=python2.5 will use the python2.5 interpreter
   to create the new environment.  The default is the
   interpreter that virtualenv was installed with
   (like ``/usr/bin/python``)

.. option:: --clear

   Clear out the non-root install and start from scratch.

.. option:: --system-site-packages

   Give the virtual environment access to the global
   site-packages.

.. option:: --always-copy

   Always copy files rather than symlinking.

.. option:: --relocatable

   Make an EXISTING virtualenv environment relocatable.
   This fixes up scripts and makes all .pth files relative.

.. option:: --unzip-setuptools

   Unzip Setuptools when installing it.

.. option:: --no-setuptools

   Do not install setuptools in the new virtualenv.

.. option:: --no-pip

   Do not install pip in the new virtualenv.

.. option:: --no-wheel

   Do not install wheel in the new virtualenv.

.. option:: --extra-search-dir=DIR

   Directory to look for setuptools/pip distributions in.
   This option can be specified multiple times.

.. option:: --prompt=PROMPT

   Provides an alternative prompt prefix for this
   environment.

.. option:: --download

   Download preinstalled packages from PyPI.

.. option:: --no-download

   Do not download preinstalled packages from PyPI.

.. option:: --no-site-packages

   DEPRECATED. Retained only for backward compatibility.
   Not having access to global site-packages is now the
   default behavior.

.. option:: --distribute
.. option:: --setuptools

   Legacy; now have no effect.  Before version 1.10 these could be used
   to choose whether to install Distribute_ or Setuptools_ into the created
   virtualenv. Distribute has now been merged into Setuptools, and the
   latter is always installed.

.. _Distribute: https://pypi.python.org/pypi/distribute
.. _Setuptools: https://pypi.python.org/pypi/setuptools


Configuration
-------------

Environment Variables
~~~~~~~~~~~~~~~~~~~~~

Each command line option is automatically used to look for environment
variables with the name format ``VIRTUALENV_<UPPER_NAME>``. That means
the name of the command line options are capitalized and have dashes
(``'-'``) replaced with underscores (``'_'``).

For example, to automatically use a custom Python binary instead of the
one virtualenv is run with you can also set an environment variable::

  $ export VIRTUALENV_PYTHON=/opt/python-3.3/bin/python
  $ virtualenv ENV

It's the same as passing the option to virtualenv directly::

  $ virtualenv --python=/opt/python-3.3/bin/python ENV

This also works for appending command line options, like ``--find-links``.
Just leave an empty space between the passed values, e.g.::

  $ export VIRTUALENV_EXTRA_SEARCH_DIR="/path/to/dists /path/to/other/dists"
  $ virtualenv ENV

is the same as calling::

  $ virtualenv --extra-search-dir=/path/to/dists --extra-search-dir=/path/to/other/dists ENV

.. envvar:: VIRTUAL_ENV_DISABLE_PROMPT

   Any virtualenv created when this is set to a non-empty value will not have
   it's :ref:`activate` modify the shell prompt.


Configuration File
~~~~~~~~~~~~~~~~~~

virtualenv also looks for a standard ini config file. On Unix and Mac OS X
that's ``$HOME/.virtualenv/virtualenv.ini`` and on Windows, it's
``%APPDATA%\virtualenv\virtualenv.ini``.

The names of the settings are derived from the long command line option,
e.g. the option :option:`--python <-p>` would look like this::

  [virtualenv]
  python = /opt/python-3.3/bin/python

Appending options like :option:`--extra-search-dir` can be written on multiple
lines::

  [virtualenv]
  extra-search-dir =
      /path/to/dists
      /path/to/other/dists

Please have a look at the output of :option:`--help <-h>` for a full list
of supported options.


Extending Virtualenv
--------------------


Creating Your Own Bootstrap Scripts
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

While this creates an environment, it doesn't put anything into the
environment. Developers may find it useful to distribute a script
that sets up a particular environment, for example a script that
installs a particular web application.

To create a script like this, call
:py:func:`virtualenv.create_bootstrap_script`, and write the
result to your new bootstrapping script.

.. py:function:: create_bootstrap_script(extra_text)

   Creates a bootstrap script from ``extra_text``, which is like
   this script but with extend_parser, adjust_options, and after_install hooks.

This returns a string that (written to disk of course) can be used
as a bootstrap script with your own customizations. The script
will be the standard virtualenv.py script, with your extra text
added (your extra text should be Python code).

If you include these functions, they will be called:

.. py:function:: extend_parser(optparse_parser)

   You can add or remove options from the parser here.

.. py:function:: adjust_options(options, args)

   You can change options here, or change the args (if you accept
   different kinds of arguments, be sure you modify ``args`` so it is
   only ``[DEST_DIR]``).

.. py:function:: after_install(options, home_dir)

   After everything is installed, this function is called. This
   is probably the function you are most likely to use. An
   example would be::

       def after_install(options, home_dir):
           if sys.platform == 'win32':
               bin = 'Scripts'
           else:
               bin = 'bin'
           subprocess.call([join(home_dir, bin, 'easy_install'),
                            'MyPackage'])
           subprocess.call([join(home_dir, bin, 'my-package-script'),
                            'setup', home_dir])

   This example immediately installs a package, and runs a setup
   script from that package.

Bootstrap Example
~~~~~~~~~~~~~~~~~

Here's a more concrete example of how you could use this::

    import virtualenv, textwrap
    output = virtualenv.create_bootstrap_script(textwrap.dedent("""
    import os, subprocess
    def after_install(options, home_dir):
        etc = join(home_dir, 'etc')
        if not os.path.exists(etc):
            os.makedirs(etc)
        subprocess.call([join(home_dir, 'bin', 'easy_install'),
                         'BlogApplication'])
        subprocess.call([join(home_dir, 'bin', 'paster'),
                         'make-config', 'BlogApplication',
                         join(etc, 'blog.ini')])
        subprocess.call([join(home_dir, 'bin', 'paster'),
                         'setup-app', join(etc, 'blog.ini')])
    """))
    f = open('blog-bootstrap.py', 'w').write(output)

Another example is available `here`__.

.. __: https://github.com/socialplanning/fassembler/blob/master/fassembler/create-venv-script.py
