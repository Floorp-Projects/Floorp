Partner attribution
===================
.. _partner attribution:

In contrast to :ref:`partner repacks`, attributed builds only differ from the normal Firefox
builds by the adding a string in the dummy windows signing certificate. We support doing this for
full installers but not stub. The parameters of the string are carried into the telemetry system,
tagging an install into a cohort of users. This a lighter weight process because we don't
repackage or re-sign the builds.

Parameters & Scheduling
-----------------------

Partner attribution uses a number of parameters to control how they work:

* ``release_enable_partner_attribution``
* ``release_partner_config``
* ``release_partner_build_number``
* ``release_partners``

The enable parameter is a boolean, a simple on/off switch. We set it in shipit's
`is_partner_enabled() <https://github.com/mozilla-releng/shipit/blob/main/api/src/shipit_api/admin/release.py#L93>`_ when starting a
release. It's true for Firefox betas >= b8 and releases, but otherwise false, the same as
partner repacks.

``release_partner_config`` is a dictionary of configuration data which drives the task generation
logic. It's usually looked up during the release promotion action task, using the Github
GraphQL API in the `get_partner_config_by_url()
<python/taskgraph.util.html#taskgraph.util.partners.get_partner_config_by_url>`_ function, with the
url defined in `taskcluster/ci/config.yml <https://searchfox.org/mozilla-central/search?q=partner-urls&path=taskcluster%2Fci%2Fconfig.yml&case=true&regexp=false&redirect=true>`_.

``release_partner_build_number`` is an integer used to create unique upload paths in the firefox
candidates directory, while ``release_partners`` is a list of partners that should be
attributed (i.e. a subset of the whole config). Both are intended for use when respinning a partner after
the regular Firefox has shipped. More information on that can be found in the
`RelEng Docs <https://moz-releng-docs.readthedocs.io/en/latest/procedures/misc-operations/off-cycle-partner-repacks-and-funnelcake.html>`_.

``release_partners`` is shared with partner repacks but we don't support doing both at the same time.


Configuration
-------------

This is done using an ``attribution_config.yml`` file which next lives to the ``default.xml`` used
for partner repacks. There are no repos for each partner, the whole configuration exists in the one
file because the amount of information to be tracked is much smaller.

An example config looks like this:

.. code-block:: yaml

    defaults:
        medium: distribution
        source: mozilla
    configs:
        -   campaign: sample
            content: sample-001
            locales:
            - en-US
            - de
            - ru
            platforms:
            - win64-shippable
            - win32-shippable

The four main parameters are ``medium, source, campaign, content``, of which the first two are
common to all attributions. The combination of ``campaign`` and ``content`` should be unique
to avoid confusion in telemetry data. They correspond to the repo name and sub-directory in partner repacks,
so avoid any overlap between values in partner repacks and atrribution.
The optional parameters of ``variation``, and ``experiment`` may also be specified.

Non-empty lists of locales and platforms are required parameters (NB the `-shippable` suffix should be used on
the platforms).

The Firefox installers are uploaded into the `candidates directory
<https://archive.mozilla.org/pub/firefox/candidates/>`_.


Repacking process
-----------------

Attribution only has two kinds:

* attribution - add attribution code to the regular builds
* beetmover - move the files to a partner-specific destination

Attribution
^^^^^^^^^^^

* kinds: ``release-partner-attribution``
* platforms: Any Windows, runs on linux
* upstreams: ``repackage-signing`` ``repackage-signing-l10n``

There is one task, calling out to `python/mozrelease/mozrelease/attribute_builds.py
<https://hg.mozilla.org/releases/mozilla-release/file/default/python/mozrelease/mozrelease/attribute_builds.py>`_.

It takes as input the repackage-signing and repackage-signing-l10n artifacts, which are all
target.exe full installers. The ``ATTRIBUTION_CONFIG`` environment variable controls the script.
It produces more target.exe installers.

The size of ``ATTRIBUTION_CONFIG`` variable may grow large if the number of configurations
increases, and it may be necesssary to pass the content of ``attribution_config.yml`` to the
script instead, or via an artifact of the promotion task.

Beetmover
^^^^^^^^^

* kinds: ``release-partner-attribution-beetmover``
* platforms: N/A, scriptworker
* upstreams: ``release-partner-attribution``

Moves and renames the artifacts to their public location in the `candidates directory
<https://archive.mozilla.org/pub/firefox/candidates/>`_.

Each task will have the ``project:releng:beetmover:action:push-to-partner`` and
``project:releng:beetmover:bucket:release`` scopes.  There's a partner-specific
code path in `beetmoverscript
<https://github.com/mozilla-releng/scriptworker-scripts/tree/master/beetmoverscript>`_.
