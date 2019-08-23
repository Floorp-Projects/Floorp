Taskcluster Configuration
=========================

Taskcluster requires configuration of many resources to correctly support Firefox CI.
Many of those span multiple projects (branches) instead of riding the trains.

Global Settings
---------------

The data behind configuration of all of these resources is kept in the `ci-configuration`_ repository.
The files in this repository are intended to be self-documenting, but one of particular interest is ``projects.yml``, which describes the needs of each project.

Configuration Implementation
----------------------------

Translation of `ci-configuration`_ to Taskcluster resources, and updating those resources, is handled by `ci-admin`_.
This is a small Python application with commands to generate the expected configuration, compare the expected to actual configuration, and apply the expected configuration.
Only the ``apply`` subcommand requires elevated privileges.

This tool automatically annotates all managed resources with "DO NOT EDIT", warning users of the administrative UI that changes made through the UI may be reverted.

Changing Configuration
----------------------

To change Taskcluster configuration, make patches to `ci-configuration`_ or (if necessary) `ci-admin`_, using the Firefox Build System :: Task Configuration Bugzilla component.
Part of the landing process is for someone with administrative scopes to apply the resulting configuration.

You can test your patches with something like this, assuming ``.`` is a checkout of the `ci-configuration`_ repository containing your changes:

.. code-block: shell

  ci-admin diff --ci-configuration-directory .

.. _ci-configuration: https://hg.mozilla.org/ci/ci-configuration/file
.. _ci-admin: https://hg.mozilla.org/ci/ci-admin/file
