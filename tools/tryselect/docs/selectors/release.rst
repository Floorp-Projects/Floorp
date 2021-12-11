Release Selector
================

This command configures the tree in preparation for doing a staging release,
and pushes the result to try. The changes that that are made include:

- Updating the version number.
- Applying the migrations that are done as part of merge day.
- Disabling repacking most locales. (This can be disabled by passing ``--no-limit-locales``).

For staging a beta release, run the following (with an appropriate version number):

.. code-block:: shell

   $ mach try release --version 64.0b5 --migration central-to-beta

For staging a final release (rc or patch), run the following (with an appropriate version number)

.. code-block:: shell

   $ mach try release --version 64.0 --migration central-to-beta --migration beta-to-release

Once the decision task is on the push is complete, you can start the release
through `staging ship-it instance <https://shipit.staging.mozilla-releng.net/new>`_\ [#shipit]_.

.. note::

   If pushing from beta or release, the corresponding migration should not be
   passed, as they have already been applied.

.. [#shipit] This is only available to release engineering and release management (as of 2018-10-15).
