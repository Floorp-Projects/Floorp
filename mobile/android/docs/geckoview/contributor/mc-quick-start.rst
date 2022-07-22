.. -*- Mode: rst; fill-column: 80; -*-

===========================
Mozilla Central Quick Start
===========================

Table of contents
=================

.. contents:: :local:

Firefox Developer Git Quick Start Guide
=======================================

Getting setup to as a first time Mozilla contributor is hard. There are
plenty of guides out there to help you get started as a contributor, but
many of the new contributor guides out of date often more current ones
are aimed at more experienced contributors. If you want to review these
guides, you can find several linked to from
:ref:`Working on Firefox <Working on Firefox>`.

This guide will take you through setting up as a contributor to
``mozilla-central``, the Firefox main repository, as a git user.

Setup
-----

The first thing you will need is to install Mercurial as this is the VCS
that ``mozilla-central`` uses.

.. _mac-0:

Mac
~~~

.. _homebrew-0:

Homebrew
^^^^^^^^

.. code:: bash

   brew install mercurial

macports
^^^^^^^^

.. code:: bash

   sudo port install mercurial

Linux
~~~~~

apt
^^^

.. code:: bash

   sudo apt-get install mercurial

Alternatively you can install `Mercurial directly <https://www.mercurial-scm.org/wiki/Download>`_.

Check that you have successfully installed Mercurial by running:

.. code:: bash

   hg --version

If you are an experienced git user and are unfamiliar with Mercurial,
you may want to install ``git-cinnabar``. Cinnabar is a git remote
helper that allows you to interact with Mercurial repos using git
semantics.

git-cinnabar
------------

There is a Homebrew install option for ``git-cinnabar``, but this did
not work for me, nor did the installer option. Using these tools, when I
tried to clone the Mercurial repo it hung and did not complete. I had to
do a manual install before I could use ``git-cinnabar`` successfully to
download a Mercurial repo. If you would like to try either of these
option, however, here they are:

.. _mac-1:

Mac
~~~~~

.. _homebrew-1:

Homebrew
^^^^^^^^

.. code:: bash

   brew install git-cinnabar

All Platforms
~~~~~~~~~~~~~

Installer
^^^^^^^^^

.. code:: bash

   git cinnabar download

Manual installation
^^^^^^^^^^^^^^^^^^^

.. code:: bash

   git clone https://github.com/glandium/git-cinnabar.git && cd git-cinnabar
   make
   export PATH="$PATH:/somewhere/git-cinnabar"
   echo 'export PATH="$PATH:/somewhere/git-cinnabar"' >> ~/.bash_profile
   git cinnabar download

``git-cinnabar``\ ’s creator, `glandium <https://glandium.org/>`_, has
written a number of posts about setting up for Firefox Development with
git. This `post <https://glandium.org/blog/?page_id=3438>`_ is the one
that has formed the basis for this walkthrough.

In synopsis:

-  initialize an empty git repository

.. code:: bash

   git init gecko && cd gecko

-  Configure git:

.. code:: bash

   git config fetch.prune true
   git config push.default upstream

-  Add remotes for your repositories. There are several to choose from,
   ``central``, ``inbound``, ``beta``, ``release`` etc. but in reality,
   if you plan on using Phabricator, which is Firefox’s preferred patch
   submission system, you only need to set up ``central``. It might be
   advisable to have access to ``inbound`` however, if you want to work
   on a version of Firefox that is queued for release. This guide will
   be focused on Phabricator.

.. code:: bash

   git remote add central hg::https://hg.mozilla.org/mozilla-central -t branches/default/tip
   git remote add inbound hg::https://hg.mozilla.org/integration/mozilla-inbound -t branches/default/tip
   git remote set-url --push central hg::ssh://hg.mozilla.org/mozilla-central
   git remote set-url --push inbound hg::ssh://hg.mozilla.org/integration/mozilla-inbound

-  Expose the branch tip to get quick access with some easy names.

.. code:: bash

   git config remote.central.fetch +refs/heads/branches/default/tip:refs/remotes/central/default
   git config remote.inbound.fetch +refs/heads/branches/default/tip:refs/remotes/inbound/default

-  Setup a remote for the try server. The try server is an easy way to
   test a patch without actually checking the patch into the core
   repository. Your code will go through the same tests as a
   ``mozilla-central`` push, and you’ll be able to download builds if
   you wish.

.. code:: bash

   git remote add try hg::https://hg.mozilla.org/try
   git config remote.try.skipDefaultUpdate true
   git remote set-url --push try hg::ssh://hg.mozilla.org/try
   git config remote.try.push +HEAD:refs/heads/branches/default/tip

-  Now update all the remotes. This performs a ``git fetch`` on all the
   remotes. Mozilla Central is a *large* repository. Be prepared for
   this to take a very long time.

.. code:: bash

   git remote update

All that’s left to do now is pick a bug to fix and `submit a
patch <contributing-to-mc.html>`__.
