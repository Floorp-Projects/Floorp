Release Promotion
=================

Release promotion allows us to ship the same compiled binaries that we've
already tested.

In the olden days, we used to re-compile our release builds with separate
configs, which led to release-specific bugs which weren't caught by continuous
integration tests. This meant we required new builds at release time, which
increased the end-to-end time for a given release significantly. Release
promotion removes these anti-patterns.

By running our continuous integration tests against our shippable builds, we
have a higher degree of confidence at release time. By separating the build
phase tasks (compilation, packaging, and related tests) from the promotion
phase tasks, we can schedule each phase at their own independent cadence, as
needed, and the end-to-end time for promotion is reduced significantly.

.. _release promotion phases

Release Promotion Phases
------------------------

Currently, we have the ``build``, ``promote``, ``push``, and ``ship`` phases.

The ``build`` phase creates ``shippable builds``. These optimize for correctness
over speed, and are designed to be of shipping quality, should we decide to
ship that revision of code. These are triggered on push on release branches.
(We also schedule ``depend`` builds on most branches, which optimize for speed
over correctness, so we can detect new code bustage sooner.)

The ``promote`` phase localizes the shippable builds, creates any update MARs,
and populates the candidates directories on S3. (On Android, we rebuild, because
we haven't been successful repacking the APK.)

The ``push`` phase pushes the candidates files to the appropriate release directory
on S3.

The ``ship`` phase ships or schedules updates to users. These are often at a
limited rollout percentage or are dependent on multiple downstream signoffs to
fully ship.

In-depth relpro guide
---------------------

.. toctree::

    release-promotion-action
    balrog
    partials
    pushapk
    signing
    partner-repacks
