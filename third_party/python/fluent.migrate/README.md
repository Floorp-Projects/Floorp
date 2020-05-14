Fluent Migration Tools
======================

Programmatically create Fluent files from existing content in both legacy
and Fluent formats. Use recipes written in Python to migrate content for each
of your localizations.

`migrate-l10n` is a CLI script which uses the `fluent.migrate` module under
the hood to run migrations on existing translations.

`validate-l10n-recipe` is a CLI script to test a migration recipe for common
errors, without trying to apply it.

Installation
------------

Install from PyPI:

    pip install fluent.migrate[hg]

If you only want to use the `MigrationContext` API, you can drop the
requirement on `python-hglib`:

    pip install fluent.migrate

Usage
-----

Migrations consist of _recipes_, which are applied to a _localization repository_, based on _template files_.
You can find recipes for Firefox in `mozilla-central/python/l10n/fluent_migrations/`,
the reference repository is [gecko-strings](https://hg.mozilla.org/l10n/gecko-strings/) or _quarantine_.
You apply those migrations to l10n repositories in [l10n-central](https://hg.mozilla.org/l10n-central/), or to `gecko-strings` for testing.

The migrations are run as python modules, so you need to have their file location in `PYTHONPATH`.

An example would look like

    $ migrate-l10n --lang it --reference-dir gecko-strings --localization-dir l10n-central/it bug_1451992_preferences_sitedata bug_1451992_preferences_translation

Contact
-------

 - mailing list: https://lists.mozilla.org/listinfo/tools-l10n
 - bugzilla: [Open Bugs](https://bugzilla.mozilla.org/buglist.cgi?component=Fluent%20Migration&product=Localization%20Infrastructure%20and%20Tools&bug_status=__open__) - [New Bug](https://bugzilla.mozilla.org/enter_bug.cgi?product=Localization%20Infrastructure%20and%20Tools&component=Fluent%20Migration)
