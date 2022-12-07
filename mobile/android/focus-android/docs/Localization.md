Firefox Focus for Android is getting localized on [Pontoon](https://pontoon.mozilla.org/projects/focus-for-android/).

For converting between Android XML files and Gettext PO files (to be consumed by Pontoon) we use a local, slightly modified version of [android2po](https://github.com/miracle2k/android2po) (See `tools/l10n/android2po`).

## Committing new strings

New strings are flagged with the [strings-approved](https://github.com/mozilla-mobile/focus-android/labels/strings-approved) label, and should be slated for the current milestone. These strings should be added to [app/src/main/res/values/strings.xml](https://github.com/mozilla-mobile/focus-android/blob/master/app/src/main/res/values/strings.xml) - include localization notes if necessary for clarity. Remove the `strings-approved` label once the strings have been added.

If you're replacing or deleting a string, you also need to **export** and then **import** strings again. (For now, the import script resets the state of the l10n repo so you need to comment out the lines of `import-strings.sh` that hard-reset the l10n repo.)

## Localization Scripts Setup

1. Python, Pip and Git need to be installed.

   You can optionally set up a virtualenv for running all these scripts in:
   ```shell
   virtualenv <new_env>
   source <new_env>/bin/activate
   ```

   Remember to exit the virtualenv when you've finished with everything.
   ```shell
   deactivate
   ```

1. Install the dependencies.

   ```shell
   pip install -r tools/l10n/android2po/requirements.txt
   ```

## Exporting strings for translation

1. You should have a fork of the [focus-android-l10n repo](https://github.com/mozilla-l10n/focus-android-l10n) in order to make a PR.

1. Export the string changes into a branch against the focus-android-l10n repo.
   ```shell
   sh tools/l10n/export-strings.sh
   ```

   This will clone the l10n Focus repo into the Focus directory if it doesn't already exist, update to the most recent commit, and create a new branch with the string changes per locale in separate commits. The l10n strings repo will be located at `$topsrcdir/l10n-repo`, and the new branch will be of the format `export-YYYY-MM-DD`.

1. Verify that the changes made to `locales/templates/app.pot` are the strings added.

   Note: Do not be alarmed if there are whitespace changes unrelated to your string changes in the individual language locale files - these are a result of different serializers being used when reading/writing the localization files. There may also be changes in the header items, that are related to the metadata changes made by the POEdit editor.

1. Add your fork as a remote if it doesn't already exist.

   `git remote add <username> git@github.com:<username>/focus-android-l10n.git`

1. Push the branch to your forked repo and make a PR against the mozilla-l10n/focus-android-l10n repo.

   `git push <username> <export-branch>`

    Since many contributors modify the l10n files, you need to make sure to coordinate with the l10n team because a PR can go stale very quickly and result in conflicts.

## Manually importing translated strings

There is a Taskcluster script to do this twice a week, but here are the instructions if you need to manually import strings.

1. Run the import script with `sh tools/l10n/import-strings.sh`. This will update the l10n repo (or clone it if it doesn't exist) and import the strings.

    Troubleshooting:

    `Error: [values-<locale>/strings.xml] missing placeholder in translation, key: <key_name>`

    * This is an error. A placeholder (%1$s) was omitted in the translation. Remove the problematic line/string from the file (it will default to the non-localized string). Also file a [bug](https://bugzilla.mozilla.org/enter_bug.cgi?product=Mozilla%20Localizations) and select the appropriate locale, and cc delphine on the bug.

    `Warning: [values-<locale>/strings.xml] number of placeholders not matching, key: <key_name>`

    * This is just a warning, and no action needs to be taken.

1. Verify the changes and then commit and push the updated XML files to the app repository.

## Adding new locales

`tools/l10n/locales.py` is the master list for the locales that will be included in the release. Usually, the locales that need to be added are already included in `ADDITIONAL_SCREENSHOT_LOCALES`, so they just need to be moved to `RELEASE_LOCALES`.
