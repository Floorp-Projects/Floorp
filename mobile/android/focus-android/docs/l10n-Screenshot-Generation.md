# l10n Screenshot Generation

This wiki describes the steps to generate l10n screenshots on a device.

## Prerequisites
You need to install [Fastlane Screengrab](https://docs.fastlane.tools/getting-started/android/screenshots/) in order to run screenshots test.  Make sure all dependencies are installed as well.  Gradle dependencies and screengrab config file is in the root folder of the repo.

Open a console, go to Focus folder, and type ```export LC_ALL="en_US.UTF-8"``` . Fastlane needs this value to be set, otherwise the test will crash on some locales.

Enable [this](https://github.com/mozilla-mobile/focus-android/blob/master/app/src/androidTest/java/org/mozilla/focus/screenshots/ScreenshotTest.java#L71) line and disable the line below. Make sure the import is properly added.  ```HostScreencapScreenshotStrategy``` was developed to generate screenshots on Taskcluster, but it is currently not being used. In order to take device screenshots, you need ```UiAutomatorScreenshotStrategy```.

## Build
Build test runner and apk by executing ```./gradlew assembleFocusArmDebug assembleFocusArmDebugAndroidTest```

## Configure Screengrabfile
```Screengrabfile``` contains the configuration for Screengrab execution, including locale list.  The locale list can be found in: https://pontoon.mozilla.org/projects/focus-for-android/. Make sure the locales array is up to date.

Note, that, ``` clear_previous_screenshots``` is set to true initially, but if you ran the tests and a handful of locales have failed, instead of rerunning the whole test, you should update locales array to only contain failed locales, and set this value to false.  This way you won't lose successful locale screenshots.

## Execute Test
Make sure the device is connected via USB, and run ```bundle exec fastlane screengrab run``` command.  Currently, this will take about 3 hours to run through all locales.

## Compress outputs
The screenshots are located in ```fastlane/metadata/android``` folder.  Go to this folder, and resize the image to reduce the file size by running ```find . -name "*.png" | xargs mogrify -resize 50%```

On OS X, you can compress the file size further by using [ImageOptim](https://imageoptim.com/mac).  ImageOptim has a bug where too many files are loaded, it slows down and eventually cannot process images.  To circumvent this issue, ```find . -type f -iname \*png -print0 | xargs -0 -t -n 100 /Applications/ImageOptim.app/Contents/MacOS/ImageOptim``` command can be run on ```android``` folder, where it'll open only 100 images, close, and reopen with next 100 images.

Rename ```screenshots.html``` to ```index.html```
Upload the screenshots to https://github.com/npark-mozilla/npark-mozilla.github.io repo, if needed for general viewing.  Or you can choose to upload to Google drive and share link.
