=====================
Version Control in CI
=====================

Upgrading Mercurial
===================

Upgrading Mercurial in CI requires touching a handful of different
components.

Vendored robustcheckout
-----------------------

The ``robustcheckout`` Mercurial extension is used throughout CI to
perform clones and working directory updates. The canonical home of
the extension is in the
https://hg.mozilla.org/hgcustom/version-control-tools repository
at the path ``hgext/robustcheckout/__init__.py``.


When upgrading Mercurial, the ``robustcheckout`` extension should also
be updated to ensure it is compatible with the version of Mercurial
being upgraded to. Typically, one simply copies the latest version
from ``version-control-tools`` into the vendored locations.

The locations are as follows:

- In-tree: ``testing/mozharness/external_tools/robustcheckout.py``
- Treescript: ``https://github.com/mozilla-releng/scriptworker-scripts/blob/master/treescript/treescript/py2/robustcheckout.py``
- build-puppet: ``https://github.com/mozilla-releng/build-puppet/blob/master/modules/mercurial/files/robustcheckout.py``
- ronin_puppet: ``https://github.com/mozilla-platform-ops/ronin_puppet/blob/master/modules/mercurial/files/robustcheckout.py``
- OpenCloudConfig: ``https://github.com/mozilla-releng/OpenCloudConfig/blob/master/userdata/Configuration/FirefoxBuildResources/robustcheckout.py``


Debian Packages for Debian and Ubuntu Based Docker Images
---------------------------------------------------------

``taskcluster/kinds/packages/debian.yml`` and ``taskcluster/kinds/packages/ubuntu.yml``
define custom Debian packages for Mercurial. These are installed in various
Docker images.

To upgrade Mercurial, typically you just need to update the source URL
and its hash in this file.


Windows AMIs
------------

https://github.com/mozilla-releng/OpenCloudConfig defines the Windows
environment for various Windows AMIs used by Taskcluster. Several of
the files reference a ``mercurial-x.y.z-*.msi`` installer. These references
will need to be updated to the Mercurial version being upgraded to.

The ``robustcheckout`` extension is also vendored into this repository
at ``userdata/Configuration/FirefoxBuildResources/robustcheckout.py``. It
should also be updated if needed.

Puppet Maintained Hosts
-----------------------

Some hosts (namely macOS machines) are managed by Puppet and Puppet is used
to install Mercurial.

Puppet code lives in the https://github.com/mozilla-releng/build-puppet repository.
Relevant files are in ``modules/mercurial/``,
``modules/packages/manifests/mozilla/mozilla-python27-mercurial-debian/``,
and ``modules/packages/manifests/mozilla/py27_mercurial*``. A copy of
``robustcheckout`` is also vendored at
``modules/mercurial/files/robustcheckout.py``.

.. note::

   The steps to upgrade Mercurial in Puppet aren't currently captured here.
   Someone should capture those...
