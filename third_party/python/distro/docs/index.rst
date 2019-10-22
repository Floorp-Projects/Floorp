
.. _distro official repo: https://github.com/nir0s/distro
.. _distro issue tracker: https://github.com/nir0s/distro/issues
.. _open issues on missing test data: https://github.com/nir0s/distro/issues?q=is%3Aissue+is%3Aopen+label%3A%22help+wanted%22>


**distro** package (Linux Distribution) version |version|
*********************************************************

Official distro repository: `distro official repo`_

Overview and motivation
=======================

.. automodule:: distro

If you want to jump into the API description right away, read about the
`consolidated accessor functions`_.

Compatibility
=============

The ``distro`` package is supported on Python 2.7, 3.4+ and PyPy, and on
any Linux or *BSD distribution that provides one or more of the `data sources`_
used by this package.

This package is tested on Python 2.7, 3.4+ and PyPy, with test data that
mimics the exact behavior of the data sources of
`a number of Linux distributions <https://github.com/nir0s/distro/tree/master/tests/resources/distros>`_.

If you want to add test data for more distributions, please
create an issue in the `distro issue tracker`_
and provide the following information in the issue:

* The content of the `/etc/os-release` file, if any.
* The file names and content of the `/etc/*release` and `/etc/*version` files, if any.
* The output of the command: `lsb_release -a`, if available.
* The file names and content of any other files you are aware of that provide
  useful information about the distro.

There are already some `open issues on missing test data`_.


Data sources
============

The ``distro`` package implements a robust and inclusive way of retrieving the
information about a Linux distribution based on new standards and old methods,
namely from these data sources:

* The `os-release file`_, if present.

* The `lsb_release command output`_, if the lsb_release command is available.

* The `distro release file`_, if present.

* The `uname command output`_, if present.


Access to the information
=========================

This package provides three ways to access the information about a Linux
distribution:

* `Consolidated accessor functions`_

  These are module-global functions that take into account all data sources in
  a priority order, and that return information about the current Linux
  distribution.

  These functions should be the normal way to access the information.

  The precedence of data sources is applied for each information item
  separately. Therefore, it is possible that not all information items returned
  by these functions come from the same data source. For example, on a
  distribution that has an lsb_release command that returns the
  "Distributor ID" field but not the "Codename" field, and that has a distro
  release file that specifies a codename inside, the distro ID will come from
  the lsb_release command (because it has higher precedence), and the codename
  will come from the distro release file (because it is not provided by the
  lsb_release command).

  Examples: :func:`distro.id` for retrieving
  the distro ID, or :func:`ld.info` to get the machine-readable part of the
  information in a more aggregated way, or :func:`distro.linux_distribution` with
  an interface that is compatible to the original
  :py:func:`platform.linux_distribution` function, supporting a subset of its
  parameters.

* `Single source accessor functions`_

  These are module-global functions that take into account a single data
  source, and that return information about the current Linux distribution.

  They are useful for distributions that provide multiple inconsistent data
  sources, or for retrieving information items that are not provided by the
  consolidated accessor functions.

  Examples: :func:`distro.os_release_attr` for retrieving a single information
  item from the os-release data source, or :func:`distro.lsb_release_info` for
  retrieving all information items from the lsb_release command output data
  source.

* `LinuxDistribution class`_

  The :class:`distro.LinuxDistribution` class provides the main code of this
  package.

  This package contains a private module-global :class:`distro.LinuxDistribution`
  instance with default initialization arguments, that is used by the
  consolidated and single source accessor functions.

  A user-defined instance of the :class:`distro.LinuxDistribution` class allows
  specifying the path names of the os-release file and distro release file and
  whether the lsb_release command should be used or not. That is useful for
  example when the distribution information from a chrooted environment
  is to be retrieved, or when a distro has multiple distro release files and
  the default algorithm uses the wrong one.


Consolidated accessor functions
===============================

This section describes the consolidated accessor functions.
See `access to the information`_ for a discussion of the different kinds of
accessor functions.

.. autofunction:: distro.linux_distribution
.. autofunction:: distro.id
.. autofunction:: distro.name
.. autofunction:: distro.version
.. autofunction:: distro.version_parts
.. autofunction:: distro.major_version
.. autofunction:: distro.minor_version
.. autofunction:: distro.build_number
.. autofunction:: distro.like
.. autofunction:: distro.codename
.. autofunction:: distro.info

Single source accessor functions
================================

This section describes the single source accessor functions.
See `access to the information`_ for a discussion of the different kinds of
accessor functions.

.. autofunction:: distro.os_release_info
.. autofunction:: distro.lsb_release_info
.. autofunction:: distro.distro_release_info
.. autofunction:: distro.os_release_attr
.. autofunction:: distro.lsb_release_attr
.. autofunction:: distro.distro_release_attr

LinuxDistribution class
=======================

This section describes the access via the :class:`distro.LinuxDistribution` class.
See `access to the information`_ for a discussion of the different kinds of
accessor functions.

.. autoclass:: distro.LinuxDistribution
   :members:
   :undoc-members:

Normalization tables
====================

These translation tables are used to normalize the parsed distro ID values
into reliable IDs. See :func:`distro.id` for details.

They are documented in order to show for which distros a normalization is
currently defined.

As a quick fix, these tables can also be extended by the user by appending new
entries, should the need arise. If you have a need to get these tables
extended, please make an according request in the `distro issue tracker`_.

.. autodata:: distro.NORMALIZED_OS_ID
.. autodata:: distro.NORMALIZED_LSB_ID
.. autodata:: distro.NORMALIZED_DISTRO_ID

Os-release file
===============

The os-release file is looked up using the path name ``/etc/os-release``. Its
optional additional location ``/usr/lib/os-release`` is ignored.

The os-release file is expected to be encoded in UTF-8.

It is parsed using the standard Python :py:mod:`shlex` package, which treats it
like a shell script.

The attribute names found in the file are translated to lower case and then
become the keys of the information items from the os-release file data source.
These keys can be used to retrieve single items with the
:func:`distro.os_release_attr` function, and they are also used as keys in the
dictionary returned by :func:`distro.os_release_info`.

The attribute values found in the file are processed using shell rules (e.g.
for whitespace, escaping, and quoting) before they become the values of the
information items from the os-release file data source.

If the attribute "VERSION" is found in the file, the distro codename is
extracted from its value if it can be found there. If a codename is found, it
becomes an additional information item with key "codename".

See the `os-release man page
<http://www.freedesktop.org/software/systemd/man/os-release.html>`_
for a list of possible attributes in the file.

**Examples:**

1.  The following os-release file content:

    .. sourcecode:: shell

        NAME='Ubuntu'
        VERSION="14.04.3 LTS, Trusty Tahr"
        ID=ubuntu
        ID_LIKE=debian
        PRETTY_NAME="Ubuntu 14.04.3 LTS"
        VERSION_ID="14.04"
        HOME_URL="http://www.ubuntu.com/"
        SUPPORT_URL="http://help.ubuntu.com/"
        BUG_REPORT_URL="http://bugs.launchpad.net/ubuntu/"

    results in these information items:

    ===============================  ==========================================
    Key                              Value
    ===============================  ==========================================
    name                             "Ubuntu"
    version                          "14.04.3 LTS, Trusty Tahr"
    id                               "ubuntu"
    id_like                          "debian"
    pretty_name                      "Ubuntu 14.04.3 LTS"
    version_id                       "14.04"
    home_url                         "http://www.ubuntu.com/"
    support_url                      "http://help.ubuntu.com/"
    bug_report_url                   "http://bugs.launchpad.net/ubuntu/"
    codename                         "Trusty Tahr"
    ===============================  ==========================================

2.  The following os-release file content:

    .. sourcecode:: shell

        NAME="Red Hat Enterprise Linux Server"
        VERSION="7.0 (Maipo)"
        ID="rhel"
        ID_LIKE="fedora"
        VERSION_ID="7.0"
        PRETTY_NAME="Red Hat Enterprise Linux Server 7.0 (Maipo)"
        ANSI_COLOR="0;31"
        CPE_NAME="cpe:/o:redhat:enterprise_linux:7.0:GA:server"
        HOME_URL="https://www.redhat.com/"
        BUG_REPORT_URL="https://bugzilla.redhat.com/"

        REDHAT_BUGZILLA_PRODUCT="Red Hat Enterprise Linux 7"
        REDHAT_BUGZILLA_PRODUCT_VERSION=7.0
        REDHAT_SUPPORT_PRODUCT="Red Hat Enterprise Linux"
        REDHAT_SUPPORT_PRODUCT_VERSION=7.0

    results in these information items:

    ===============================  ==========================================
    Key                              Value
    ===============================  ==========================================
    name                             "Red Hat Enterprise Linux Server"
    version                          "7.0 (Maipo)"
    id                               "rhel"
    id_like                          "fedora"
    version_id                       "7.0"
    pretty_name                      "Red Hat Enterprise Linux Server 7.0 (Maipo)"
    ansi_color                       "0;31"
    cpe_name                         "cpe:/o:redhat:enterprise_linux:7.0:GA:server"
    home_url                         "https://www.redhat.com/"
    bug_report_url                   "https://bugzilla.redhat.com/"
    redhat_bugzilla_product          "Red Hat Enterprise Linux 7"
    redhat_bugzilla_product_version  "7.0"
    redhat_support_product           "Red Hat Enterprise Linux"
    redhat_support_product_version   "7.0"
    codename                         "Maipo"
    ===============================  ==========================================

Lsb_release command output
==========================

The lsb_release command is expected to be in the PATH, and is invoked as
follows:

.. sourcecode:: shell

    lsb_release -a

The command output is expected to be encoded in UTF-8.

Only lines in the command output with the following format will be used:

    ``<attr-name>: <attr-value>``

Where:

* ``<attr-name>`` is the name of the attribute, and
* ``<attr-value>`` is the attribute value.

The attribute names are stripped from surrounding blanks, any remaining blanks
are translated to underscores, they are translated to lower case, and then
become the keys of the information items from the lsb_release command output
data source.

The attribute values are stripped from surrounding blanks, and then become the
values of the information items from the lsb_release command output data
source.

See the `lsb_release man page
<http://refspecs.linuxfoundation.org/LSB_5.0.0/LSB-Core-generic/
LSB-Core-generic/lsbrelease.html>`_
for a description of standard attributes returned by the lsb_release command.

**Examples:**

1.  The following lsb_release command output:

    .. sourcecode:: text

        No LSB modules are available.
        Distributor ID: Ubuntu
        Description:    Ubuntu 14.04.3 LTS
        Release:        14.04
        Codename:       trusty

    results in these information items:

    ===============================  ==========================================
    Key                              Value
    ===============================  ==========================================
    distributor_id                   "Ubuntu"
    description                      "Ubuntu 14.04.3 LTS"
    release                          "14.04"
    codename                         "trusty"
    ===============================  ==========================================

2.  The following lsb_release command output:

    .. sourcecode:: text

        LSB Version:    n/a
        Distributor ID: SUSE LINUX
        Description:    SUSE Linux Enterprise Server 12 SP1
        Release:        12.1
        Codename:       n/a

    results in these information items:

    ===============================  ==========================================
    Key                              Value
    ===============================  ==========================================
    lsb_version                      "n/a"
    distributor_id                   "SUSE LINUX"
    description                      "SUSE Linux Enterprise Server 12 SP1"
    release                          "12.1"
    codename                         "n/a"
    ===============================  ==========================================

Distro release file
===================

Unless specified with a particular path name when using the
:class:`distro.LinuxDistribution` class, the distro release file is found by using
the first match in the alphabetically sorted list of the files matching the
following path name patterns:

* ``/etc/*-release``
* ``/etc/*_release``
* ``/etc/*-version``
* ``/etc/*_version``

where the following special path names are excluded:

* ``/etc/debian_version``
* ``/etc/system-release``
* ``/etc/os-release``

and where the first line within the file has the expected format.

The algorithm to sort the files alphabetically is far from perfect, but the
distro release file has the least priority as a data source, and it is expected
that distributions provide one of the other data sources.

The distro release file is expected to be encoded in UTF-8.

Only its first line is used, and it is expected to have the following format:

    ``<name> [[[release] <version_id>] (<codename>)]``

Where:

* square brackets indicate optionality,
* ``<name>`` is the distro name,
* ``<version_id>`` is the distro version, and
* ``<codename>`` is the distro codename.

The following information items can be found in a distro release file
(shown with their keys and data types):

* ``id`` (string):  Distro ID, taken from the first part of the file name
  before the hyphen (``-``) or underscore (``_``).

  Note that the distro ID is not normalized or translated to lower case at this
  point; this happens only for the result of the :func:`distro.id` function.

* ``name`` (string):  Distro name, as found in the first line of the file.

* ``version_id`` (string):  Distro version, as found in the first line of the
  file. If not found, this information item will not exist.

* ``codename`` (string):  Distro codename, as found in the first line of the
  file. If not found, this information item will not exist.

  Note that the string in the codename field is not always really a
  codename. For example, openSUSE returns "x86_64".

**Examples:**

1.  The following distro release file ``/etc/centos-release``:

    .. sourcecode:: text

        CentOS Linux release 7.1.1503 (Core)

    results in these information items:

    ===============================  ==========================================
    Key                              Value
    ===============================  ==========================================
    id                               "centos"
    name                             "CentOS Linux"
    version_id                       "7.1.1503"
    codename                         "Core"
    ===============================  ==========================================

2.  The following distro release file ``/etc/oracle-release``:

    .. sourcecode:: text

        Oracle Linux Server release 7.1

    results in these information items:

    ===============================  ==========================================
    Key                              Value
    ===============================  ==========================================
    id                               "oracle"
    name                             "Oracle Linux Server"
    version_id                       "7.1"
    ===============================  ==========================================

3.  The following distro release file ``/etc/SuSE-release``:

    .. sourcecode:: text

        openSUSE 42.1 (x86_64)

    results in these information items:

    ===============================  ==========================================
    Key                              Value
    ===============================  ==========================================
    id                               "SuSE"
    name                             "openSUSE"
    version_id                       "42.1"
    codename                         "x86_64"
    ===============================  ==========================================

