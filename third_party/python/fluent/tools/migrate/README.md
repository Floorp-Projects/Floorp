# Migration Tools

`migrate-l10n.py` is a CLI script which uses the `fluent.migrate` module under
the hood to run migrations on existing translations.

## Examples

The `examples/` directory contains a number of sample migrations. To run them
you'll need at least one clone of a localization repository, e.g. from
https://hg.mozilla.org/l10n-central/.

Amend your `PYTHONPATH` to make sure that all `fluent.*` modules can be found:

    $ export PYTHONPATH=$(pwd)/../..:$PYTHONPATH

Then run migrations passing the `examples` directory as the reference:

    $ ./migrate-l10n.py --lang it --reference-dir examples --localization-dir ~/moz/l10n-central/it examples.about_dialog examples.about_downloads examples.bug_1291693

Here's what the output should look like:

    Annotating /home/stas/moz/l10n-central/it
    Running migration examples.bug_1291693
      Writing to /home/stas/moz/l10n-central/it/browser/branding/official/brand.ftl
        Committing changeset: Bug 1291693 - Migrate the menubar to FTL, part 1
      Writing to /home/stas/moz/l10n-central/it/browser/menubar.ftl
      Writing to /home/stas/moz/l10n-central/it/browser/toolbar.ftl
      Writing to /home/stas/moz/l10n-central/it/browser/branding/official/brand.ftl
        Committing changeset: Bug 1291693 - Migrate the menubar to FTL, part 2
    Running migration examples.about_dialog
      Writing to /home/stas/moz/l10n-central/it/browser/about_dialog.ftl
        Committing changeset: Migrate about:dialog, part 1
    Running migration examples.about_downloads
      Writing to /home/stas/moz/l10n-central/it/mobile/about_downloads.ftl
        Committing changeset: Migrate about:download in Firefox for Android, part 1
      Writing to /home/stas/moz/l10n-central/it/mobile/about_downloads.ftl
        Committing changeset: Migrate about:download in Firefox for Android, part 2
      Writing to /home/stas/moz/l10n-central/it/mobile/about_downloads.ftl
        Committing changeset: Migrate about:download in Firefox for Android, part 3
