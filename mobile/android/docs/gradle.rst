.. -*- Mode: rst; fill-column: 80; -*-

=================================
 The Fennec Gradle configuration
=================================

The Fennec Gradle configuration has three major facets:

1. separation into Gradle projects;
2. Android-Gradle build configurations using `flavorDimensions`;
3. integration into the larger `moz.build` build system.

Separation into Gradle projects
===============================

The Fennec source code is separated into multiple Gradle projects.  Right now,
there are only a few: `:app`, `:geckoview`, `:thirdparty`, etc.  Over time, pieces
of the Fennec source code will be extracted into separate, more-or-less
stand-alone Gradle projects (e.g., `:services`, `:stumbler`, `:media`) and use
modern techniques such as dependency injection to configure themselves at
runtime.

The `:omnijar` project is special and exists only to support integration with
Android Studio.

The details of the Gradle projects are reflected in the root `settings.gradle`
and the `**/build.gradle` files throughout the tree.

Android-Gradle build configurations
===================================

The Fennec `:app` project uses the Android-Gradle build plugin
`flavorDimensions` feature set to support many different configurations.  The
Gradle integration must support many often conflicting requirements; the flavor
dimensions chosen support these requirements.

Version 3.0+ of the Android-Gradle build plugin improves support for "variant
dependencies".  This makes it easier for a consuming application (for us,
`:app`) to use the appropriate configuration of consumed libraries (for us,
`:geckoview`).  This allows us to simplify the logic around packaging the Gecko
libraries and Omnijar.

The details of the Android-Gradle build configurations are reflected
`**/build.gradle` files throughout the tree, in the
`mobile/android/gradle/*.gradle` files, and in the configuration baked into
`mobile/android/gradle.configure`.

The notable flavor dimensions are:

audience
--------

The `audience` flavor dimension determines who the build is for.  "local"
audiences are developers: they should get extra logging and as much support for
building Fennec as possible.  In particular, "local" audiences get support for
modifying the Android manifest and regenerating the pre-processed `strings.xml`
files while building with Gradle from within Android Studio.

"official" audiences are end users: they should get Mozilla's official branding
and have security-sensitive developer features disabled.  The "official"
audience corresponds roughly to the `MOZILLA_OFFICIAL=1` build setting.

**Builds shipped to the Google Play Store are always for "official" audiences.**

geckoBinaries
-------------

For deep historical reasons, Mozilla's build system has multiple stages, the
most important of which are the build stage and the package stage.  During the
build stage, the Gecko compiled libraries (e.g., `libxul.so`) and the Omnijar
have not yet been built.  These libraries are only available during the package
stage.  Gradle builds always want to include the Gecko libraries and the
Omnijar.

To accommodate the different stages, the build stage always invokes
"withoutGeckoBinaries" Gradle configurations. These configurations don't expect
or use the Gecko libraries or Omnijar.  At the moment, the package stage also
invokes "withoutGeckoBinaries", but in the future, the package stage will invoke
"withGeckoBinaries" Gradle configurations to simplify the packaging of libraries
and the omnijar.

**Local developers almost always want to build "withGeckoBinaries", so that the
APK files produced can be run on device.**

minApi
------

At various times in the past, Fennec has supported APK splits, producing APKs
that support only specific Android versions.  While this is not used at this
time, there are certain developer options (i.e., options that should only apply
to "local" audiences) that *also* depend on the target Android version.  This
flavor dimension allows to opt in to those options, improving the speed of
development.

Integration into the larger `moz.build` build system
====================================================

The details of the Gradle integration into the larger `moz.build` system are
mostly captured in configuration baked into `mobile/android/gradle.configure`.
This configuration is reflected in the Android-specific `mach android *`
commands defined in `mobile/android/mach_commands.py`.
