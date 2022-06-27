.. -*- Mode: rst; fill-column: 80; -*-

=================
Contributor Guide
=================

Table of contents
=================

.. contents:: :local:

GeckoView Contributor Quick Start Guide
=======================================

This is a guide for developers who want to contribute to the GeckoView
project. If you want to get started using GeckoView in your app then you
should refer to the
`wiki <https://wiki.mozilla.org/Mobile/GeckoView#Get_Started>`_.

Get set up with Mozilla Central
-------------------------------

The GeckoView codebase is part of the main Firefox tree and can be found
in ``mozilla-central``. You will need to get set up as a contributor to
Firefox in order to contribute to GeckoView. To get set up with
``mozilla-central``, follow the `Quick Start Guide for Git
Users <mc-quick-start.html>`_, or the `Contributing to the Mozilla code
base <https://firefox-source-docs.mozilla.org/setup/contributing_code.html>`_
guide and `Firefox Contributors’ Quick Reference
<https://firefox-source-docs.mozilla.org/contributing/contribution_quickref.html>`_
for Mercurial users.

Once you have a copy of ``mozilla-central``, you will need to build
GeckoView.

Bootstrap Gecko
---------------

Bootstrap configures everything for GeckoView and Fennec (Firefox for Android) development.

-  Ensure you have ``mozilla-central`` checked out. If this is the first
   time you are doing this, it may take some time.

.. code:: bash

   git checkout central/default

If you are on Windows, you will need to install the
`Java 1.8 SDK <https://adoptopenjdk.net/?variant=openjdk8>`__.

If you are on a mac, you will need to have the Xcode build tools
installed. You can do this by either `installing
Xcode <https://developer.apple.com/xcode/>`__ or installing only the
tools from the command line by running ``xcode-select --install`` and
following the on screen instructions. Use the ``--no-interactive``
argument to automatically accept any license agreements.

.. code:: bash

   ./mach [--no-interactive] bootstrap

-  Choose option \`4. Firefox for Android\` for GeckoView development.
   This will give you a version of Gecko configured for Android that has
   not bundled the native code into embedded libraries so you can amend
   the code.
-  Say Y to all configuration options
-  Once ``mach bootstrap`` is complete, it will automatically write
   the configuration into a new ``mozconfig`` file. If you already
   have a ``mozconfig``, ``mach`` will instead output new configuration
   that you should append to your existing file.

Build from the command line
---------------------------

In order to pick up the configuration changes we just made we need to
build from the command line. This will update generated sources, compile
native code, and produce GeckoView AARs and example and test APKs.

.. code:: bash

   ./mach build

Build Using Android Studio
--------------------------

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

   |alt text 1|

Now you’re set up and ready to go.

**Important: at this time, building from Android Studio or directly from
Gradle does not (re-)compile native code, including C++ and Rust.** This
means you will need to run ``mach build`` yourself to pick up changes to
native code. `Bug
1509539 <https://bugzilla.mozilla.org/show_bug.cgi?id=1509539>`_ tracks
making Android Studio and Gradle do this automatically.

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

Performing a bug fix
--------------------

One you have got GeckoView building and running, you will want to start
contributing. There is a general guide to `Performing a Bug Fix for Git
Developers <contributing-to-mc.html>`_ for you to follow. To contribute to
GeckoView specifically, you will need the following additional
information.

Running tests and linter locally
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To ensure that your patch does not break existing functionality in
GeckoView, you can run the junit test suite with the following command

::

   ./mach geckoview-junit

This command also allows you to run individual tests or test classes,
e.g.

::

   ./mach geckoview-junit org.mozilla.geckoview.test.NavigationDelegateTest
   ./mach geckoview-junit org.mozilla.geckoview.test.NavigationDelegateTest#loadUnknownHost

If your patch makes a GeckoView JavaScript module, you should run ESLint
as well:

::

   ./mach lint -l eslint mobile/android/modules/geckoview/

To see information on other options, simply run
``./mach geckoview-junit --help``; of particular note for dealing with
intermittent test failures are ``--repeat N`` and
``--run-until-failure``, both of which do exactly what you’d expect.

Updating the changelog and API documentation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If the patch that you want to submit changes the public API for
GeckoView, you must ensure that the API documentation is kept up to
date. To check whether your patch has altered the API, run the following
command.

.. code:: bash

   ./mach lint --linter android-api-lint

The output of this command will inform you if any changes you have made
break the existing API. Review the changes and follow the instructions
it provides.

If the linter asks you to update the changelog, please ensure that you
follow the correct format for changelog entries. Under the heading for
the next release version, add a new entry for the changes that you are
making to the API, along with links to any relevant files, and bug
number e.g.

::

   - Added [`GeckoRuntimeSettings.Builder#aboutConfigEnabled`][71.12] to control whether or
     not `about:config` should be available.
     ([bug 1540065]({{bugzilla}}1540065))

   [71.12]: {{javadoc_uri}}/GeckoRuntimeSettings.Builder.html#aboutConfigEnabled(boolean)

Submitting to the ``try`` server
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

It is advisable to run your tests before submitting your patch. You can
do this using Mozilla’s ``try`` server. To submit a GeckoView patch to
``try`` before submitting it for review, type:

.. code:: bash

   ./mach try auto

This will automatically select tests to run from our suite. If your patch
passes on ``try`` you can be (fairly) confident that it will land successfully
after review.

Tagging a reviewer
~~~~~~~~~~~~~~~~~~

When submitting a patch to Phabricator, if you know who you want to
review your patch, put their Phabricator handle against the
``reviewers`` field.

If you don’t know who to tag for a review in the Phabricator submission
message, leave the field blank and, after submission, follow the link to
the patch in Phabricator and scroll to the bottom of the screen until
you see the comment box.

- Select the ``Add Action`` drop down and pick the ``Change Reviewers`` option.
- In the presented box, add ``geckoview-reviewers``. Selecting this group as the reviewer will notify all the members of the GeckoView team there is a patch to review.
- Click ``Submit`` to submit the reviewer change request.

Include GeckoView as a dependency
---------------------------------

If you want to include a development version of GeckoView as a
dependency inside another app, you must link to a local copy. There are
several ways to achieve this, but the preferred way is to use Gradle’s
*dependency substitution* mechanism, for which there is first-class
support in ``mozilla-central`` and a pattern throughout Mozilla’s
GeckoView-consuming ecosystem.

The good news is that ``mach build`` produces everything you need, so
that after the configuration below, you should find that the following
commands rebuild your local GeckoView and then consume your local
version in the downstream project.

.. code:: sh

   cd /path/to/mozilla-central && ./mach build
   cd /path/to/project && ./gradlew assembleDebug

**Be sure that your ``mozconfig`` specifies the correct ``--target``
argument for your target device.** Many projects use “ABI splitting” to
include only the target device’s native code libraries in APKs deployed
to the device. On x86-64 and aarch64 devices, this can result in
GeckoView failing to find any libraries, because valid x86 and ARM
libraries were not included in a deployed APK. Avoid this by setting
``--target`` to the exact ABI that your device supports.

Dependency substiting your local GeckoView into a Mozilla project
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Most GeckoView-consuming projects produced by Mozilla support dependency
substitution via ``local.properties``. These projects include:

- `Fenix <https://github.com/mozilla-mobile/fenix>`_
- `reference-browser <https://github.com/mozilla-mobile/reference-browser>`_
- `android-components <https://github.com/mozilla-mobile/android-components>`_
- `Firefox Reality <https://github.com/MozillaReality/FirefoxReality>`_

Simply edit (or create) the file ``local.properties`` in the project
root and include a line like:

.. code:: properties

   dependencySubstitutions.geckoviewTopsrcdir=/path/to/mozilla-central

The default object directory – the one that a plain ``mach build``
discovers – will be used. You can optionally specify a particular object
directory with an additional line like:

.. code:: properties

   dependencySubstitutions.geckoviewTopobjdir=/path/to/object-directory

With these lines, the GeckoView-consuming project should use the
GeckoView AAR produced by ``mach build`` in your local
``mozilla-central``.

**Remember to remove the lines in ``local.properties`` when you want to
return to using the published GeckoView builds!**

Dependency substituting your local GeckoView into a non-Mozilla project
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In projects that don’t have first-class support for dependency
substitution already, you can do the substitution yourself. See the
documentation in
`substitue-local-geckoview.gradle <https://hg.mozilla.org/mozilla-central/file/tip/substitute-local-geckoview.gradle>`_,
but roughly: in each Gradle project that consumes GeckoView, i.e., in
each ``build.gradle`` with a
``dependencies { ... 'org.mozilla.geckoview:geckoview-...' }`` block,
include lines like:

.. code:: groovy

   ext.topsrcdir = "/path/to/mozilla-central"
   ext.topobjdir = "/path/to/object-directory" // Optional.
   apply from: "${topsrcdir}/substitute-local-geckoview.gradle"

**Remember to remove the lines from all ``build.gradle`` files when you
want to return to using the published GeckoView builds!**

Next Steps
----------

-  Get started with `Native Debugging <native-debugging.html>`_

.. |alt text| image:: ../assets/DisableInstantRun.png
.. |alt text 1| image:: ../assets/GeckoViewStructure.png
