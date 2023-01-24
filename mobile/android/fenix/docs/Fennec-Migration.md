Project board: https://github.com/orgs/mozilla-mobile/projects/40

# 📱 Testing

⚠️ **Warning**: Replacing a _Fennec_ (Firefox for Android) installation with _Fenix_ (Firefox Preview) can (and at the time of writing this definitely **will**) result in **DATA LOSS**. Do not replace an installation of Fennec (Firefox for Android) that contains data you do not want to risk losing (e.g. open tabs, history, bookmarks, top sites, ..).

## Release

The following links point to the latest *Fenix* (Firefox Preview) builds (Nightly; from `main`) that are setup to **replace** a *Fennec* (Firefox for Android) release version (`org.mozilla.firefox`).

* [ARM64/Aarch64 devices (64 bit; Android 5+)](https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/project.mobile.fenix.v2.fennec-production.latest/artifacts/public/build/arm64-v8a/geckoBeta/target.apk)
* [ARM devices (32 bit; Android 5+)](https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/project.mobile.fenix.v2.fennec-production.latest/artifacts/public/build/armeabi-v7a/geckoBeta/target.apk)
* [x86_64 devices (64 bit; Android 5+)](https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/project.mobile.fenix.v2.fennec-production.latest/artifacts/public/build/x86_64/geckoBeta/target.apk)
* [x86 devices (32 bit; Android 5+)](https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/project.mobile.fenix.v2.fennec-production.latest/artifacts/public/build/x86/geckoBeta/target.apk)

## Beta

The following links point to the latest *Fenix* (Firefox Preview) builds (Nightly; from `main`) that are setup to **replace** a *Fennec Beta* (Firefox for Android - Beta) release version (`org.mozilla.firefox.beta`).

* [ARM64/Aarch64 devices (64 bit; Android 5+)](https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/project.mobile.fenix.v2.fennec-beta.latest/artifacts/public/build/arm64-v8a/geckoBeta/target.apk)
* [ARM devices (32 bit; Android 5+)](https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/project.mobile.fenix.v2.fennec-beta.latest/artifacts/public/build/armeabi-v7a/geckoBeta/target.apk)
* [x86_64 devices (64 bit; Android 5+)](https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/project.mobile.fenix.v2.fennec-beta.latest/artifacts/public/build/x86_64/geckoBeta/target.apk)
* [x86 devices (32 bit; Android 5+)](https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/project.mobile.fenix.v2.fennec-beta.latest/artifacts/public/build/x86/geckoBeta/target.apk)

## Nightly

The following links point to the latest *Fenix* (Firefox Preview) builds (Nightly; from `main`) that are setup to **replace** a *Fennec Nightly* (Firefox for Android - Nightly) release version (`org.mozilla.fennec_aurora`).

* [ARM64/Aarch64 devices (64 bit; Android 5+)](https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/mobile.v2.fenix.nightly.latest.arm64-v8a/artifacts/public/build/arm64-v8a/target.apk)
* [ARM devices (32 bit; Android 5+)](https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/mobile.v2.fenix.nightly.latest.armeabi-v7a/artifacts/public/build/armeabi-v7a/target.apk)
* [x86_64 devices (64 bit; Android 5+)](https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/mobile.v2.fenix.nightly.latest.x86_64/artifacts/public/build/x86_64/target.apk)
* [x86 devices (32 bit; Android 5+)](https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/mobile.v2.fenix.nightly.latest.x86/artifacts/public/build/x86/target.apk)

# 📝 Changelog

The data migration work is tracked on the following project board:
https://github.com/orgs/mozilla-mobile/projects/40

* **2019-09-05** - The first [migration builds](https://tools.taskcluster.net/index/project.mobile.fenix.v2.fennec-production/latest) are available now. A Firefox for Android (release) installation can be replaced with them. No actual migration code is in those builds yet. The replaced build is a "clean" Fenix installation.
* **2019-10-22** - First iteration of migration code to migrate history, bookmarks and open tabs landed in builds.
* **2019-11-02** - Firefox Account users remain logged in after migrating to Fenix.

# 💻 Development

When working on migration code it is helpful to have a local Fennec build and a local Fenix build that can replace the Fennec build. The following manual setup is needed to achieve that.

In the example commands below, we assume you are replacing a **Fennec Nightly** build with a **Fenix Nightly** build.

## Fennec

Download the latest version of Fennec:

 - [ARM64/Aarch64 devices (64 bit; Android 5+)](https://archive.mozilla.org/pub/mobile/nightly/latest-mozilla-esr68-android-aarch64/)
 - [ARM devices (32 bit; Android 5+)](https://archive.mozilla.org/pub/mobile/nightly/latest-mozilla-esr68-android-api-16/)
 - [x86_64 devices (64 bit; Android 5+)](https://archive.mozilla.org/pub/mobile/nightly/latest-mozilla-esr68-android-x86_64/)
 - [x86 devices (32 bit; Android 5+)](https://archive.mozilla.org/pub/mobile/nightly/latest-mozilla-esr68-android-x86/)

Strip out the original signature:

```
zip --delete fennec.apk "META-INF/*"
```

Re-sign the APK with your own debug key (that will also be used later for Fenix):

```
jarsigner -verbose -keystore ~/.android/debug.keystore -storepass android -keypass android fennec.apk androiddebugkey
```

You can now install this APk that is ready to be used for migration:

```
adb install fennec.apk
```

## Fenix

In the `app/build.gradle`, add the following line in the correct scope to sign your app with the same debug key used on the Fennec APK:

```groovy
android {
  buildTypes {
    fennecNightly {
      signingConfig signingConfigs.debug
    }
  }
}
```

Follow the build instructions in the [README](https://github.com/mozilla-mobile/fenix/blob/main/README.md) to get a Fenix build setup.

Now select the `geckoNightlyFennecNightly` build variant in Android Studio and deploy it. This build should have replaced your Fennec build now.

## Sample browser

When working on migration code that lives in the [Android Components repository](https://github.com/mozilla-mobile/android-components) it can be helpful to replace a local Fennec build with the sample browser (instead of Fenix). The following setup is needed for that.

Add the sharedUserId to the AndroidManifest.xml of sample browser:

```XML
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    xmlns:tools="http://schemas.android.com/tools"
    android:sharedUserId="org.mozilla.fennec_$USERNAME.sharedID"
    [..]
```

Modify the application id in `build.gradle` of the `samples-browser` module and use a versionCode that is higher than your Fennec build (`2100000000` is the highest allowed version code and therefore should always work).

```Groovy
    defaultConfig {
        applicationId "org.mozilla.fennec_$USERNAME"
        [..]
        versionCode 2100000000
```



Click on "Sync Project with Gradle Files" and deploy the sample browser. This build should have replaced your Fennec build now.

## Emulator snapshots

When testing migration code the following process has to be repeated multiple times:

* (1) Uninstall an already existing Fennec/Fenix installation
* (2) Install Fennec
* (3) Use Fennec to create the necessary data for testing the migration
* (4) Install Fenix
* (5) Debug / Test

Steps (1) to (3) can be quite time consuming. Emulator snapshots can help with that:

* Launch an emulator and perform steps 1 to 3. You may need to modify your Fennec build to create an X86 build for your emulator (target `i686-linux-android`).
* Click on the three dot menu in the emulator toolbar and select "Snapshots". Press the "Take Snapshot" button. If needed give you snapshot a descriptive name in case you will need to have multiple "test snapshots".
* With the "Play" button you can always reset your emulator to that state and repeat the migration process.