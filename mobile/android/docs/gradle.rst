.. -*- Mode: rst; fill-column: 80; -*-

======================
 Building with Gradle
======================

Instructions
============

.. code-block:: shell

  ./mach build && ./mach package
  ./mach gradle build

The debug APK will be at
``$OBJDIR/mobile/android/gradle/app/build/outputs/apk/app-debug.apk``.

The ``$OBJDIR/mobile/android/gradle`` directory can be imported into IntelliJ as
follows:

- File > Import Project...
- [select ``$OBJDIR/mobile/android/gradle``]
- Import project from external model > Gradle
- [select Use customizable Gradle wrapper]

When prompted, do not add any files to git.  You may need to re-open the
project, or restart IntelliJ, to pick up a compiler language-level change.

Technical overview
==================

Caveats
-------

* The Gradle build will "succeed" but crash on start up if the object directory
  has not been properly packaged.
* Changes to preprocessed source code and resources (namely, ``strings.xml.in``
  and the accompanying DTD files) may not be recognized.
* There's minimal support for editing JavaScript.
* There's no support for editing C/C++.

How the Gradle project is laid out
----------------------------------

To the greatest extent possible, the Gradle configuration lives in the object
directory.

At the time of writing, there are three main sub-modules: *app*, *base*, and
*thirdparty*, and several smaller sub-modules.

*app* is the Fennec wrapper; it generates the **org.mozilla.fennec.R** resource
package.  *base* is the Gecko code; it generates the **org.mozilla.gecko.R**
resource package.  Together, *app* and *base* address the "two package
namespaces" that has plagued Fennec from day one.

Due to limitations in the Android Gradle plugin, all test code is shoved into
the *app* module.  (The issue is that, at the time of writing, there is no
support for test-only APKs.)  For no particular reason, the compiled C/C++
libraries are included in the *app* module; they could be included in the *base*
module.  I expect *base* to rebuilt slightly more frequently than *app*, so I'm
hoping this choice will allow for faster incremental builds.

*thirdparty* is the external code we use in Fennec; it's built as an Android
library but uses no resources.  It's separate simply to allow the build system
to cache the compiled and pre-dexed artifacts, hopefully allowing for faster
incremental builds.

Recursive make backend details
------------------------------

The ``mobile/android/gradle`` directory writes the following into
``$OBJDIR/mobile/android/gradle``:

1) the Gradle wrapper;
2) ``gradle.properties``;
3) symlinks to certain source and resource directories.

The Gradle wrapper is written to make it easy to build with Gradle from the
object directory.  The wrapper is `intended to be checked into version
control`_.

``gradle.properties`` is the single source of per-object directory Gradle
configuration, and provides the Gradle configuration access to
configure/moz.build variables.

The symlinks are not necessary for the Gradle build itself, but they prevent
nested directory errors and incorrect Java package scoping when the Gradle
project is imported into IntelliJ.  Because IntelliJ treats the Gradle project
as authoritative, it's not sufficient to fix these manually in IntelliJ after
the initial import -- IntelliJ reverts to the Gradle configuration after every
build.  Since there aren't many symlinks, I've done them in the Makefile rather
than at a higher level of abstraction (like a moz.build definition, or a custom
build backend).  In future, I expect to be able to remove all such symlinks by
making our in-tree directory structures agree with what Gradle and IntelliJ
expect.

.. _intended to be checked into version control: http://www.gradle.org/docs/current/userguide/gradle_wrapper.html
