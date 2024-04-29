Firefox for Android
===================
.. _firefox_for_android:

Firefox for Android consists of three layers:

- GeckoView: This is a library that brings the Gecko API to Android.

- Android Components: This is a library of reusable browser building blocks.

- Frontend (Fenix): This refers to the frontend interface.

All three components can be found in ``mozilla-central``.
To contribute to any of the three, you will need to get set up as a contributor to Firefox.

Mozilla Central Setup
---------------------
.. _mozilla-central-setup:

To set up Mozilla Central, you can follow the general instructions provided in the Mozilla Source Docs:

- :ref:`Getting Set Up To Work On The Firefox Codebase <Getting Set Up To Work On The Firefox Codebase>`

Additionally, to set up specifically for mozilla-central, you can refer to the following guide:

- :ref:`Mozilla Central Quick Start <Mozilla Central Quick Start>`
- :ref:`Quick Start Guide for Git Users <contribute_with_git>`

Bootstrap
----------
.. _bootstrap-setup:

Bootstrap configures everything for GeckoView and Fenix (Firefox for Android) development.

-  Ensure you have ``mozilla-central`` checked out. If this is the first
   time you are doing this, it may take some time.

.. code:: bash

   git checkout central/default

If you are on a mac, you will need to have the Xcode build tools
installed. You can do this by either `installing
Xcode <https://developer.apple.com/xcode/>`__ or installing only the
tools from the command line by running ``xcode-select --install`` and
following the on screen instructions.

If you are on a newer mac with an Apple Silicon M2 or M3 processor,
you also need to install rosetta for backwards compatilibilty:

.. code:: bash

   softwareupdate --install-rosetta

You will need to ``bootstrap`` for GeckoView/Firefox for Android. The easiest way is to run the following command:

.. code:: bash

   ./mach --no-interactive bootstrap --application-choice="GeckoView/Firefox for Android"

.. note::

    - The ``--no-interactive`` argument will make ``bootstrap`` run start to finish without requiring any input from you. It will automatically accept any license agreements.
    - The ``--application-choice="GeckoView/Firefox for Android"`` argument is needed when using ``--no-interactive`` so that "bootstrapping" is done for the correct application (instead of the default).

    If you want to make all the selections yourself and/or read through the license agreements, you can simply run:

    .. code:: bash

        ./mach bootstrap

You should then choose one the following options:

A- You will not change or debug any C++ code:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Choose: ``3. GeckoView/Firefox for Android Artifact Mode``

Artifact mode downloads pre-built C++ components rather than building them locally, trading bandwidth for time.
(more on Artifact mode)

B- You intend to change or debug C++ code:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Choose: ``4. GeckoView/Firefox for Android``

This will build GeckoView from scratch, and take more time than the option above.

Once ``./mach bootstrap`` is complete, it will automatically write the configuration into a new ``mozconfig`` file.
If you already have a ``mozconfig``, mach will instead output a new configuration that you should append to your existing file.

Build from the command line
---------------------------
.. _build_from_cmd_line:

In order to pick up the configuration changes we just made we need to
build from the command line. This will update generated sources, compile
native code, and produce GeckoView AARs and example and test APKs.

.. code:: bash

   ./mach build

Build Using Android Studio
--------------------------
.. _build_with_android_studio:

-  Install `Android
   Studio <https://developer.android.com/studio/install>`_.
-  Choose File->Open from the toolbar
-  Navigate to the root of your ``mozilla-central`` source directory and
   click “Open”
-  Click yes if it asks if you want to use the gradle wrapper.

   -  If the gradle sync does not automatically start, select File >
      Sync Project with Gradle Files.

-  Wait for the project to index and gradle to sync. Once synced, the
   workspace will reconfigure to display the different projects.

   -  annotations contains custom Java annotations used inside GeckoView
   -  app contains geckoview build settings and omnijar. omnijar contains
      the parts of Gecko and GeckoView that are not written in Java or Kotlin
   -  geckoview is the GeckoView project. Here is all the Java files
      related to GeckoView
   -  geckoview_example is an example browser built using GeckoView.

Now you’re set up and ready to go.

**Important: at this time, building from Android Studio or directly from
Gradle does not (re-)compile native code, including C++ and Rust.** This
means you will need to run ``mach build`` yourself to pick up changes to
native code. `Bug
1509539 <https://bugzilla.mozilla.org/show_bug.cgi?id=1509539>`_ tracks
making Android Studio and Gradle do this automatically.

If you want set up code formatting for Kotlin, please reference
`IntelliJ IDEA configuration
<https://pinterest.github.io/ktlint/rules/configuration-intellij-idea/>`_.

Custom mozconfig with Android Studio
------------------------------------

Out of the box, Android Studio will use the default mozconfig file, normally
located at ``mozconfig`` in the root directory of your ``mozilla-central``
checkout.

To make Android Studio use a mozconfig in a custom location, you can add the
following to your ``local.properties``:

::

   mozilla-central.mozconfig=relative/path/to/mozconfig

Note that, when running mach from the command line, this value will be ignored,
and the mozconfig from the mach environment will be used instead.

To override the mozconfig used by mach, you can use the `MOZCONFIG` environment
variable, for example:

::

   MOZCONFIG=debug.mozconfig ./mach build

Project-Specific Instructions
------------------------------

Now that you're prepared and set up, you can access specific project instructions below:

- GeckoView: :ref:`Contributing to GeckoView <Contributing to GeckoView>`
- Android Components: `Mozilla Android Components <https://mozac.org/>`_
- Frontend: :ref:`Building Firefox for Android <Building Firefox for Android>`
