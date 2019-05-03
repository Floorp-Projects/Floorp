Try Server
==========

Try server, usually just referred to as try, is the easiest way to test a change without actually
checking anything into a core repository. The change will undergo the same kinds of builds and tests
as if it had landed on a regular integration branch, but will not get merged with mozilla-central.

Try is just another mercurial repository (like inbound, autoland or mozilla-central) with a few key
differences:

    1. Pushing new heads is allowed.
    2. It is non-publishing.

The first point means that you'll never need to pull and rebase before pushing, each push creates a
new head. In fact, the ability to push from old changesets is a very valuable property of try which
is often used for things like regression hunting.

The second point means that draft changesets (changesets that only exist in your local repository),
will remain in the draft state after pushing.  Normally when pushing to an integration branch, a
changeset gets marked 'public'. This ensures changes that are shared with others don't accidentally
get mutated.  Pushing to try doesn't actually share the changeset with anyone, so changesets remain
in the 'draft' state and they are still ok to mutate.

.. rubric:: Using Try

Before you can push to try, you'll need to have the proper credentials and do some light setup. See
the :doc:`configuration` page for more information.

The recommended way to push to try is via the ``mach try`` command. This will work with mercurial
(via the ``push-to-try`` extension) and git (via ``git-cinnabar``). The ``mach try`` command offers
a variety of different ``selectors`` which are implemented as a subcommands. See :doc:`selectors
<selectors/index>` for available list.

If no subcommand is specified, ``mach try`` will a subcommand to dispatch to. By default this is
the ``syntax`` selector. In other words, these commands are equivalent:

.. code-block:: shell

    $ mach try
    $ mach try syntax

You can choose to use a different default selector by configuring your ``~/.mozbuild/machrc`` file:

.. code-block:: ini

    [try]
    default=fuzzy


.. toctree::
  :caption: Table of Contents
  :maxdepth: 2

  configuration
  selectors/index
  presets
  tasks


.. rubric:: Indices and tables

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`

