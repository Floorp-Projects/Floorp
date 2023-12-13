.. -*- Mode: rst; fill-column: 80; -*-

=====================
Debugging Native Code
=====================

Table of contents
=================

.. contents:: :local:

Debugging Native Code in Android Studio.
========================================

If you want to work on the C++ code that powers GeckoView, you will need
to be able to perform native debugging inside Android Studio. This
article will guide you through how to do that.

If you need to get set up with GeckoView for the first time, follow the
`Quick Start Guide <geckoview-quick-start.html>`_.

Perform a debug build of Gecko.
-------------------------------

1. Edit your ``mozconfig`` file and add the following lines. These will
   ensure that the build includes debug checks and symbols.

.. code:: text

   ac_add_options --enable-debug

2. Ensure that the following lines are commented out in your
   ``mozconfig`` if present. ``./mach configure`` will not allow
   artifact builds to be enabled when generating a debug build.

.. code:: text

   # ac_add_options --enable-artifact-builds

3. To be absolutely sure that Android Studio will pick up your debug
   symbols, the first time you perform a debug build it is best to
   clobber your ``MOZ_OBJDIR``. Subsequent builds should not need this
   step.

.. code:: bash

   ./mach clobber

4. Build as usual. Because this is a debug build, and because you have
   clobbered your ``MOZ_OBJDIR``, this will take a long time. Subsequent
   builds will be incremental and take less time, so go make yourself a
   cup of your favourite beverage.

.. code:: bash

   ./mach build

Set up lldb to find your symbols
--------------------------------

Edit your ``~/.lldbinit`` file (or create one if one does not already
exist) and add the following lines.

The first line tells LLDB to enable inline breakpoints - Android Studio
will need this if you want to use visual breakpoints.

The remaining lines tell LLDB where to go to find the symbols for
debugging.

.. code:: bash

   settings set target.inline-breakpoint-strategy always
   settings append target.exec-search-paths <PATH>/objdir-android-opt/toolkit/library/build
   settings append target.exec-search-paths <PATH>/objdir-android-opt/mozglue/build
   settings append target.exec-search-paths <PATH>/objdir-android-opt/security

Set up Android Studio to perform native debugging.
==================================================

1. Edit the configuration that you want to debug by clicking
   ``Run -> Edit Configurations...`` and selecting the correct
   configuration from the options on the left hand side of the resulting
   window.
2. Select the ``Debugger`` tab.
3. Select ``Dual`` from the ``Debug type`` select box. Dual will allow
   debugging of both native and Java code in the same session. It is
   possible to use ``Native``, but it will only allow for debugging
   native code, and it’s frequently necessary to break in the Java code
   that configures Gecko and child processes in order to attach
   debuggers at the correct times.
4. Under ``Symbol Directories``, add a new path pointing to
   ``<PATH>/objdir-android-opt/toolkit/library/build``, the same path
   that you entered into your ``.lldbinit`` file.
5. Select ``Apply`` and ``OK`` to close the window.

Debug Native code in Android Studio
===================================

1. The first time you are running a debug session for your app, it’s
   best to start from a completely clean build. Click
   ``Build -> Rebuild Project`` to clean and rebuild. You can also
   choose to remove any existing builds from your emulator to be
   completely sure, but this may not be necessary.
2. If using Android Studio visual breakpoints, set your breakpoints in
   your native code.
3. Run the app in debug mode as usual.
4. When debugging Fennec or geckoview_example, you will almost
   immediately hit a breakpoint in ``ElfLoader.cpp``. This is expected.
   If you are not using Android Studio visual breakpoints, you can set
   your breakpoints here using the lldb console that is available now
   this breakpoint has been hit. To set a breakpoint, select the app tab
   (if running Dual, there will also be an ``<app> java`` tab) from the
   debug window, and then select the ``lldb`` console tab. Type the
   following into the console:

.. code:: text

   b <file>.cpp:<line number>

5. Once your breakpoints have been set, click the continue execution
   button to move beyond the ``ElfLoader`` breakpoint and your newly set
   native breakpoints should be hit. Debug as usual.

Attaching debuggers to content and other child processes
--------------------------------------------------------

Internally, GeckoView has a multi-process architecture. The main Gecko
process lives in the main Android process, but content rendering and
some other functions live in child processes. This balances load,
ensures certain critical security properties, and allows GeckoView to
recover if content processes become unresponsive or crash. However, it’s
generally delicate to debug child processes because they come and go.

The general approach is to make the Java code in the child process that
you want to debug wait for a Java debugger at startup, and then to
connect such a Java debugger manually from the Android Studio UI.

`Bug 1522318 <https://bugzilla.mozilla.org/show_bug.cgi?id=1522318>`__
added environment variables that makes GeckoView wait for Java debuggers
to attach, making this debug process more developer-friendly. See
`Configuring GeckoView for Automation <../consumer/automation.html>`__
for instructions on how to set environment variables that configure
GeckoView’s runtime environment.

Making processes wait for a Java debugger
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``set-debug-app`` command will make Android wait for a debugger before
running an app or service. e.g., to make GeckoViewExample wait, run the
following:

.. code:: shell

   adb shell am set-debug-app -w --persistent org.mozilla.geckoview_example

The above command works with child processes too, e.g. to make the GPU
process wait for a debugger, run:

.. code:: shell

   adb shell am set-debug-app -w --persistent org.mozilla.geckoview_example:gpu


Attaching a Java debugger to a waiting child process
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This is standard: follow the `Android Studio instructions <https://developer.android.com/studio/debug/index.html#attach-debugger>`_.
You must attach a Java debugger, so you almost certainly want to attach
a ``Dual`` debugger and you definitely can’t attach only a ``Native``
debugger.

Determining the correct process to attach to is a little tricky because
the mapping from process ID (pid) to process name is not always clear.
Gecko content child processes are suffixed ``:tab`` at this time.

If you attach ``Dual`` debuggers to both the main process and a content
child process, you will have four (4!) debug tabs to manage in Android
Studio, which is awkward. Android Studio doesn’t appear to configure
attached debuggers in the same way that it configures debuggers
connecting to launched Run Configurations, so you may need to manually
configure search paths – i.e., you may need to invoke the contents of
your ``lldbinit`` file in the appropriate ``lldb`` console by hand,
using an invocation like
``command source /absolute/path/to/topobjdir/lldbinit``.

Android Studio also doesn’t appear to support targeting breakpoints from
the UI (say, from clicking in a gutter) to specific debug tabs, so you
may also need to set breakpoints in the appropriate ``lldb`` console by
hand.

Managing more debug tabs may require different approaches.

Debug Native Memory Allocations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Android Studio includes a `Native Memory Profiler
<https://developer.android.com/studio/profile/memory-profiler#native-memory-profiler>`_
which works for physical devices running Android 10 and later.  In order to
track allocations correctly Gecko must be built with ``jemalloc`` disabled.
Additionally, the native memory profiler appears to only work with ``aarch64``
builds.  The following must therefore be present in your ``mozconfig`` file:

.. code:: text

   ac_add_options --target=aarch64
   ac_add_options --disable-jemalloc

The resulting profiles are symbolicated correctly in debug builds, however, you
may prefer to use a release build when profiling. Unfortunately a method to
symbolicate using local symbols from the development machine has not yet been
found, therefore in order for the profile to be symbolicated you must prevent
symbols being stripped during the build process. To do so, add the following to
your ``mozconfig``:

.. code:: text

   ac_add_options STRIP_FLAGS=--strip-debug

And the following to ``mobile/android/geckoview/build.gradle``, and additionally
to ``mobile/android/geckoview_example/build.gradle`` if profiling GeckoView
Example, or ``app/build.gradle`` if profiling Fenix, for example.

.. code:: groovy

    android {
        packagingOptions {
            doNotStrip "**/*.so"
        }
    }

Using Android Studio on Windows
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can now use :ref:`artifact builds <Understanding Artifact Builds>`
mode on `MozillaBuild environment <https://wiki.mozilla.org/MozillaBuild>`_ even if you are
not using WSL. If you want to debug GeckoView using Android Studio on
Windows, you have to set an additional environment variable via the
Control Panel to run the gradle script. The ``mach`` command sets these
variables automatically, but Android Studio cannot.

If you install MozillaBuild tools to ``C:\mozilla-build`` (default
installation path), you have to set the ``MOZILLABUILD`` environment
variable to recognize MozillaBuild installation path.

To set environment variable on Windows 10, open the ``Control Panel``
from ``Windows System``, then select ``System and Security`` -
``System`` - ``Advanced system settings`` -
``Environment Variables ...``.

To set the ``MOZILLABUILD`` variable, click ``New...`` in
``User variables for``, then ``Variable name:`` is ``MOZILLABUILD`` and
``Variable value:`` is ``C:\mozilla-build``.

You also have to append some tool paths to the ``Path`` environment
variable.

To append the variables to PATH, double click ``Path`` in
``User Variables for``, then click ``New``. And append the following
variables to ``Path``.

-  ``%MOZILLABUILD%\msys\bin``
-  ``%MOZILLABUILD%\bin``
