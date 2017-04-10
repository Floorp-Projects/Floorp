# Firefox Focus for Android

## Updating translations

Firefox Focus for Android is getting localized on [Pontoon](https://pontoon.mozilla.org/projects/focus-for-android/).

For converting between Android XML files and Gettext PO files (to be consumed by Pontoon) we use a local, slightly modified version of [android2po](https://github.com/miracle2k/android2po) (See `tools/l10n/android2po`).

### Setup

1. Python, Pip and Git need to be installed.

1. Run the `stringsSetup` gradle tasks to create a local checkout of the L10N repository (in folder `l10n-repo`) and to install the python dependencies to run the l10n scripts:

  ```shell
  ./gradlew stringsSetup
  ```

### Export strings for translation

1. Fetch the latest changes from the L10N repository and remove all local modifications with the `stringsCleanUpdate` gradle task:

  ```shell
  ./gradlew stringsCleanUpdate
  ```

1. Run the `stringsExport` gradle task to update the template and existing translations:

  ```shell
  ./gradlew stringsExport
  ```
  
1. Create separate commits for every locale using the `stringsCommit` gradle task:

  ```shell
  ./gradlew stringsCommit
  ```

1. Go to the l10n-repo folder, verify the changes and push them to the L10N repository.

### Import translated strings

1. Fetch the latest changes from the L10N repository and remove all local modifications with the `stringsCleanUpdate` gradle task:

  ```shell
  ./gradlew stringsCleanUpdate
  ```
  
1. Run the `stringsImport` gradle task to generate the Android XML files.

  ```shell
  ./gradlew stringsImport
  ```

1. Verify the changes and then commit and push the updated XML files to the app repository.