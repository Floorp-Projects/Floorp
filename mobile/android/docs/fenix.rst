Building Firefox for Android
============================

First, you'll want to `set up your machine to build Firefox </setup>`_.
Follow the instructions there, choosing "GeckoView/Firefox for Android" as
the bootstrap option.

Once you're set up and have a GeckoView build from the above, please
continue with the following steps.

1. Clone the repository and initial setup
-----------------------------------------

.. code-block:: shell

    git clone https://github.com/mozilla-mobile/firefox-android
    cd firefox-android/fenix
    echo dependencySubstitutions.geckoviewTopsrcdir=/path/to/mozilla-central > local.properties

replace `/path/to/mozilla-central` with the location of your mozilla-central/mozilla-unified source tree.

2. Build
--------

.. code-block:: shell

    export JAVA_HOME=$HOME/.mozbuild/jdk/jdk-17.0.6+10
    export ANDROID_HOME=$HOME/.mozbuild/android-sdk-<os_name>
    ./gradlew clean app:assembleDebug

`<os_name>` is either `linux`, `macosx` or `windows` depending on the OS you're building from.


For more details, check out the `more complete documentation <https://github.com/mozilla-mobile/firefox-android/tree/main/fenix>`_.

3. Run
------

From the gecko working directory:

.. code-block:: shell

    ./mach android-emulator


From the firefox-android working directory:

.. code-block:: shell

   ./gradlew :app:installFenixDebug
   "$ANDROID_HOME/platform-tools/adb" shell am start -n org.mozilla.fenix.debug/org.mozilla.fenix.debug.App
