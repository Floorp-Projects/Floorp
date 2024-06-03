.. _fenix-contributor-guide:

Building Firefox for Android
============================

Run Fenix using command line
----------------------------

As a first step, you need to set up your development environment using the instruction :ref:`here <firefox_for_android>`.


Run Fenix or other Android projects using command line
---------------------------------------------------------
.. _run_fenix_from_commandline:

From the root mozilla-central directory, you can run an emulator with the following command:

.. code-block:: shell

    ./mach android-emulator

Before building, set the paths to your Java installation and Android SDK

**macOS**

.. code-block:: shell

    export JAVA_HOME=$HOME/.mozbuild/jdk/jdk-<latest-version>/Contents/Home
    export ANDROID_HOME=$HOME/.mozbuild/android-sdk-<os_name>

**non-macOS**

.. code-block:: shell

    export JAVA_HOME=$HOME/.mozbuild/jdk/jdk-<latest-version>
    export ANDROID_HOME=$HOME/.mozbuild/android-sdk-<os_name>

From the `mobile/android/fenix` working directory, build, install and launch Fenix:

.. code-block:: shell

    ./gradlew :app:installFenixDebug
    "$ANDROID_HOME/platform-tools/adb" shell am start -n org.mozilla.fenix.debug/org.mozilla.fenix.debug.App

Run Fenix tests
-------------------

You can run tests via all the normal routes:

- For individual test files, click the little green play button at the top
- For a module/component:

   - Right click in project explorer → run all tests
   - Select from gradle tasks window
   - On command line: ``./gradlew :$module:$variant`` e.g.  ``./gradlew :feature-downloads:testDebugUnitTest``

If you see the error "Test events were not received", check your top level folder - this happens if you try and run tests in Android Components from ``mozilla-unified/mobile/android/fenix/``.
To build tests for Android Components you need to be using the ``build.gradle`` in ``mozilla-unified/mobile/android/android-components/``.

If after running tests on your Android device, you can no longer long press, this is because the connected Android tests mess around with your phone’s accessibility settings.
They set the long press delay to 3 seconds, which is an uncomfortably long time.
To fix this, go to Settings → Accessibility → Touch and hold delay, and reset this to default or short (depends on manufacturer).
