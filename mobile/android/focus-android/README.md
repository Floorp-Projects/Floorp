# Firefox Focus for Android

_Browse like no one’s watching. The new Firefox Focus automatically blocks a wide range of online trackers — from the moment you launch it to the second you leave it. Easily erase your history, passwords and cookies, so you won’t get followed by things like unwanted ads._

Getting Involved
----------------

We encourage you to participate in this open source project. We love Pull Requests, Bug Reports, ideas, (security) code reviews or any kind of positive contribution. Please read the [Community Participation Guidelines](https://www.mozilla.org/en-US/about/governance/policies/participation/).

* Issues: [https://github.com/mozilla-mobile/focus-android/issues](https://github.com/mozilla-mobile/focus-android/issues)

* IRC: [#mobile (irc.mozilla.org)](https://wiki.mozilla.org/IRC)

* Mailing list: [mobile-firefox-dev](https://mail.mozilla.org/listinfo/mobile-firefox-dev)

Build instructions
------------------

1. Clone the repository:

  ```shell
  git clone https://github.com/mozilla/focus-android
  ```

1. Import the project into Android Studio or build on the command line:

  ```shell
  ./gradlew clean app:assembleFocusWebkitDebug
  ```

1. Make sure to select the right build variant in Android Studio: **focusWebkitDebug**

Updating translations
---------------------

Focus for Android is getting localized on [Pontoon](https://pontoon.mozilla.org/projects/focus-for-android/).

### Setup

1. For converting between Android XML files and Gettext PO files (to be consumed by Pontoon) we use [android2po](https://github.com/miracle2k/android2po). To install the current release:

  ```shell
  easy_install android2po
  ```

1. Create a local checkout of the l10n repository into a folder called l10n:

  ```shell
  git clone https://github.com/mozilla-l10n/focus-android-l10n.git l10n
  ```

### Export strings for translation

1. Run the export command of android2po to generate a new template:

  ```shell
  a2po export
  ```
  
1. Go to the l10n folder, commit and push the updated template to the l10n repository.


### Import translated strings

1. Go to the l10n folder and pull the latest changes:

  ```shell
  cd l10n
  git pull -r
  ```
  
1. Go back to the root folder and run the import command of android2po

  ```shell
  cd ..
  a2po import
  ```
  
1. Commit and push the updated XML files to the app repository.
  

