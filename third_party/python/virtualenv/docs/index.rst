Virtualenv
==========

`Mailing list <http://groups.google.com/group/python-virtualenv>`_ |
`Issues <https://github.com/pypa/virtualenv/issues>`_ |
`Github <https://github.com/pypa/virtualenv>`_ |
`PyPI <https://pypi.org/project/virtualenv/>`_ |
User IRC: #pypa
Dev IRC: #pypa-dev

Introduction
------------


``virtualenv`` is a tool to create isolated Python environments. Since
Python 3.3, a subset of it has been integrated into the standard
library under the  `venv module <https://docs.python.org/3/library/venv.html>`_.
Note though, that the ``venv`` module does not offer all features of this
library (e.g. cannot create bootstrap scripts, cannot create virtual
environments for other python versions than the host python,
not relocatable, etc.). Tools in general as such still may prefer using
virtualenv for its ease of upgrading (via pip), unified handling of different
Python versions and some more advanced features.

The basic problem being addressed is one of dependencies and versions,
and indirectly permissions. Imagine you have an application that
needs version 1 of LibFoo, but another application requires version
2. How can you use both these applications?  If you install
everything into ``/usr/lib/python2.7/site-packages`` (or whatever your
platform's standard location is), it's easy to end up in a situation
where you unintentionally upgrade an application that shouldn't be
upgraded.

Or more generally, what if you want to install an application *and
leave it be*?  If an application works, any change in its libraries or
the versions of those libraries can break the application.

Also, what if you can't install packages into the global
``site-packages`` directory?  For instance, on a shared host.

In all these cases, ``virtualenv`` can help you. It creates an
environment that has its own installation directories, that doesn't
share libraries with other virtualenv environments (and optionally
doesn't access the globally installed libraries either).

.. comment: split here

.. toctree::
   :maxdepth: 2

   installation
   userguide
   reference
   development
   changes


Other Documentation and Links
-----------------------------

* `Blog announcement of virtualenv`__.

  .. __: http://blog.ianbicking.org/2007/10/10/workingenv-is-dead-long-live-virtualenv/

* James Gardner has written a tutorial on using `virtualenv with
  Pylons
  <http://wiki.pylonshq.com/display/pylonscookbook/Using+a+Virtualenv+Sandbox>`_.

* Chris Perkins created a `showmedo video including virtualenv
  <http://showmedo.com/videos/video?name=2910000&fromSeriesID=291>`_.

* Doug Hellmann's `virtualenvwrapper`_ is a useful set of scripts to make
  your workflow with many virtualenvs even easier. `His initial blog post on it`__.
  He also wrote `an example of using virtualenv to try IPython`__.

  .. _virtualenvwrapper: https://pypi.org/project/virtualenvwrapper/
  .. __: https://doughellmann.com/blog/2008/05/01/virtualenvwrapper/
  .. __: https://doughellmann.com/blog/2008/02/01/ipython-and-virtualenv/

* `Pew`_ is another wrapper for virtualenv that makes use of a different
  activation technique.

  .. _Pew: https://pypi.org/project/pew/

* `Using virtualenv with mod_wsgi
  <http://code.google.com/p/modwsgi/wiki/VirtualEnvironments>`_.

* `virtualenv commands
  <https://github.com/thisismedium/virtualenv-commands>`_ for some more
  workflow-related tools around virtualenv.

* PyCon US 2011 talk: `Reverse-engineering Ian Bicking's brain: inside pip and virtualenv
  <http://pyvideo.org/video/568/reverse-engineering-ian-bicking--39-s-brain--insi>`_.
  By the end of the talk, you'll have a good idea exactly how pip
  and virtualenv do their magic, and where to go looking in the source
  for particular behaviors or bug fixes.

Compare & Contrast with Alternatives
------------------------------------

There are several alternatives that create isolated environments:

* Python 3's `venv module <https://docs.python.org/3/library/venv.html>`_
  is recommended for projects that no longer need to support Python 2 and want
  to create just simple environments for the host python.

* ``workingenv`` (which I do not suggest you use anymore) is the
  predecessor to this library. It used the main Python interpreter,
  but relied on setting ``$PYTHONPATH`` to activate the environment.
  This causes problems when running Python scripts that aren't part of
  the environment (e.g., a globally installed ``hg`` or ``bzr``). It
  also conflicted a lot with Setuptools.

* `virtual-python
  <http://peak.telecommunity.com/DevCenter/EasyInstall#creating-a-virtual-python>`_
  is also a predecessor to this library. It uses only symlinks, so it
  couldn't work on Windows. It also symlinks over the *entire*
  standard library and global ``site-packages``. As a result, it
  won't see new additions to the global ``site-packages``.

  This script only symlinks a small portion of the standard library
  into the environment, and so on Windows it is feasible to simply
  copy these files over. Also, it creates a new/empty
  ``site-packages`` and also adds the global ``site-packages`` to the
  path, so updates are tracked separately. This script also installs
  Setuptools automatically, saving a step and avoiding the need for
  network access.

* `zc.buildout <http://pypi.org/project/zc.buildout>`_ doesn't
  create an isolated Python environment in the same style, but
  achieves similar results through a declarative config file that sets
  up scripts with very particular packages. As a declarative system,
  it is somewhat easier to repeat and manage, but more difficult to
  experiment with. ``zc.buildout`` includes the ability to setup
  non-Python systems (e.g., a database server or an Apache instance).

I *strongly* recommend anyone doing application development or
deployment use one of these tools.
