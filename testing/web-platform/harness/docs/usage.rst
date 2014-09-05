Getting Started
===============

Installing wptrunner
--------------------

The easiest way to install wptrunner is into a virtualenv, using pip::

  virtualenv wptrunner
  cd wptrunner
  source bin/activate
  pip install wptrunner

This will install the base dependencies for wptrunner, but not any
extra dependencies required to test against specific browsers. In
order to do this you must use use the extra requirements files in
``$VIRTUAL_ENV/requirements/requirements_browser.txt``. For example,
in order to test against Firefox you would have to run::

  pip install requirements/requirements_firefox.txt

If you intend to work on the code, the ``-e`` option to pip should be
used in combination with a source checkout i.e. inside a virtual
environment created as above::

  git clone https://github.com/w3c/wptrunner.git
  cd wptrunner
  pip install -e ./

In addition to the dependencies installed by pip, wptrunner requires
a copy of the web-platform-tests. This can be located anywhere on
the filesystem, but the easiest option is to put it in a sibling
directory of the wptrunner checkout called `tests`::

  git clone https://github.com/w3c/web-platform-tests.git tests

It is also necessary to generate the ``MANIFEST.json`` file for the
web-platform-tests. It is recommended to put this file in a separate
directory called ``meta``::

  mkdir meta
  cd web-platform-tests
  python tools/scripts/manifest.py ../meta/MANIFEST.json

This file needs to be regenerated every time that the
web-platform-tests checkout is updated. To aid with the update process
there is a tool called ``wptupdate``, which is described in
:ref:`wptupdate-label`.

Running the Tests
-----------------

A test run is started using the ``wptrunner`` command. By default this
assumes that tests are in a subdirectory of the current directory
called ``tests`` and the metadata is in a subdirectory called
``meta``. These defaults can be changed using either a command line
flag or a configuration file.

To specify the browser product to test against, use the ``--product``
flag. If no product is specified, the default is ``firefox`` which
tests Firefox desktop. ``wptrunner --help`` can be used to see a list
of supported products. Note that this does not take account of the
products for which the correct dependencies have been installed.

Depending on the product, further arguments may be required. For
example when testing desktop browsers ``--binary`` is commonly needed
to specify the path to the browser executable. So a complete command
line for running tests on firefox desktop might be::

  wptrunner --product=firefox --binary=/usr/bin/firefox

It is also possible to run multiple browser instances in parallel to
speed up the testing process. This is achieved through the
``--processes=N`` argument e.g. ``--processes=6`` would attempt to run
6 browser instances in parallel. Note that behaviour in this mode is
necessarily less deterministic than with ``--processes=1`` (the
default) so there may be more noise in the test results.

Further help can be obtained from::

  wptrunner --help

Output
------

wptrunner uses the :py:mod:`mozlog.structured` package for output. This
structures events such as test results or log messages as JSON objects
that can then be fed to other tools for interpretation. More details
about the message format are given in the
:py:mod:`mozlog.structured` documentation.

By default the raw JSON messages are dumped to stdout. This is
convenient for piping into other tools, but not ideal for humans
reading the output. :py:mod:`mozlog` comes with several other
formatters, which are accessible through command line options. The
general format of these options is ``--log-name=dest``, where ``name``
is the name of the format and ``dest`` is a path to a destination
file, or ``-`` for stdout. The raw JSON data is written by the ``raw``
formatter so, the default setup corresponds to ``--log-raw=-``.

A reasonable output format for humans is provided as ``mach``. So in
order to output the full raw log to a file and a human-readable
summary to stdout, one might pass the options::

  --log-raw=output.log --log-mach=-

Configuration File
------------------

wptrunner uses a ``.ini`` file to control some configuration
sections. The file has three sections; ``[products]``,
``[paths]`` and ``[web-platform-tests]``.

``[products]`` is used to
define the set of available products. By default this section is empty
which means that all the products distributed with wptrunner are
enabled (although their dependencies may not be installed). The set
of enabled products can be set by using the product name as the
key. For built in products the value is empty. It is also possible to
provide the path to a script implementing the browser functionality
e.g.::

  [products]
  chrome =
  netscape4 = path/to/netscape.py

``[paths]`` specifies the default paths for the tests and metadata,
relative to the config file. For example::

  [paths]
  tests = checkouts/web-platform-tests
  metadata = /home/example/wpt/metadata


``[web-platform-tests]`` is used to set the properties of the upstream
repository when updating the paths. ``remote_url`` specifies the git
url to pull from; ``branch`` the branch to sync against and
``sync_path`` the local path, relative to the configuration file, to
use when checking out the tests e.g.::

  [web-platform-tests]
  remote_url = https://github.com/w3c/web-platform-tests.git
  branch = master
  sync_path = sync

A configuration file must contain all the above fields; falling back
to the default values for unspecified fields is not yet supported.

The ``wptrunner`` and ``wptupdate`` commands will use configuration
files in the following order:

 * Any path supplied with a ``--config`` flag to the command.

 * A file called ``wptrunner.ini`` in the current directory

 * The default configuration file (``wptrunner.default.ini`` in the
   source directory)
