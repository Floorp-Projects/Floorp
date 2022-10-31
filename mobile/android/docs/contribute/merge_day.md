---
layout: page
title: Merge day process
permalink: /contributing/merge-day
---

## What is merge day?

Firefox and Gecko(View) are released in multiple release channels: `Nightly`, `Beta`, `Release`. Those versions are maintained in separate repositories (`mozilla-central`, `mozilla-beta`, `mozilla-release`). Right before the release of a new version of Firefox, on "merge day", those repositories get merged so that: The `Beta` version becomes the `Release` version. The `Nightly` version comes the `Beta` version and the `Nightly` version gets a higher version number.

![](https://wiki.mozilla.org/images/b/b1/Maintrepositories.png)

As an example:
* GeckoView Beta 70.0 (`mozilla-beta`) -> GeckoView Release 70.0 (`mozilla-release`)
* GeckoView Nightly 71.0 (`mozilla-central`) -> GeckoView Beta 71.0 (`mozilla-beta`)
* GeckoView Nightly 71.0 (`mozilla-central`) -> GeckoView Nightly 72.0 (`mozilla-central`)

Since the *Android Components* project uses separate components for tracking the GeckoView release channels we need to perform the same changes in the *Android Components* repository by moving the code accordingly and updating the dependency versions.

For example:

* `browser-engine-gecko-beta` (using GeckoView Beta 70.0) -> `browser-engine-gecko-release` (using GeckoView Release 70.0)
* `browser-engine-gecko-nightly` (using GeckoView Nightly 71.0) -> `browser-engine-gecko-beta` (using GeckoView Beta 71.0)
* `browser-engine-gecko-nightly` -> Starts using GeckoView 72.0

A new release happens roughly every 6 weeks. See the release calendar:
https://wiki.mozilla.org/Release_Management/Calendar

## Process

### Preparation

Before starting the "Merge day" process make sure that all new major versions for all release channels of GeckoView (nightly, beta, release) are available on
https://maven.mozilla.org/.

* Release: https://maven.mozilla.org/?prefix=maven2/org/mozilla/geckoview/geckoview/
* Beta: https://maven.mozilla.org/?prefix=maven2/org/mozilla/geckoview/geckoview-beta/
* Nightly: https://maven.mozilla.org/?prefix=maven2/org/mozilla/geckoview/geckoview-nightly/

### Beta -> Release

First we delete the existing code of the `browser-engine-gecko` component and replace it with the code of the `browser-engine-gecko-beta` component:

```
rm -rf components/browser/engine-gecko/src
cp -R components/browser/engine-gecko-beta/src components/browser/engine-gecko/
```

In addition to that we need to replace the metrics file:

```
cp -R components/browser/engine-gecko-beta/metrics.yaml components/browser/engine-gecko
```

After that we update `Gecko.kt` to use the latest GeckoView release version (used by `browser-engine-gecko`) from https://maven.mozilla.org/?prefix=maven2/org/mozilla/geckoview/geckoview/

```
vi buildSrc/src/main/java/Gecko.kt
```

Finally we build and test the component to see wether there are any issues.

```
./gradlew browser-engine-gecko:test
```

See the "Build failures" section if the build or tests fail.

If the build and test passes then commit this change.

Example commit message:
`Issue #xxxx: (Merge day) browser-engine-gecko-beta (70) -> browser-engine-gecko (70)`

### Nightly -> Beta

Now we are removing the existing code of the `browser-engine-gecko-beta` component and replace it with the code of the `browser-engine-gecko-nightly` component:

```
rm -rf components/browser/engine-gecko-beta/src
cp -R components/browser/engine-gecko-nightly/src components/browser/engine-gecko-beta
```

In addition to that we need to replace the metrics file:

```
cp -R components/browser/engine-gecko-nightly/metrics.yaml components/browser/engine-gecko-beta
```

After that we update `Gecko.kt` to use the latest GeckoView beta version (previously used by `browser-engine-gecko-nightly`) from https://maven.mozilla.org/?prefix=maven2/org/mozilla/geckoview/geckoview-beta/

```
vi buildSrc/src/main/java/Gecko.kt
```

Finally we build and test the component to see wether there are any issues.

```
./gradlew browser-engine-gecko-beta:test
```

See the "Build failures" section if the build or tests fail.

If the build and test passes then commit this change.

Example commit message:
`Issue #xxxx: (Merge day) browser-engine-gecko-nightly (71) -> browser-engine-gecko-beta (71)`


### Nightly

Finally we need to update the nightly component. For this component we do not need to copy any code and instead can update the version of the component immediately from https://maven.mozilla.org/?prefix=maven2/org/mozilla/geckoview/geckoview-nightly/

```
vi buildSrc/src/main/java/Gecko.kt
```

Again we are building and running the tests of the component:

```
./gradlew browser-engine-gecko-nightly:test
```

See the "Build failures" section if the build or tests fail.

If the build and test passes then commit this change.

Example commit message:
`Issue #xxxx: (Merge day) browser-engine-gecko-nightly -> 72.`

### Changelog

Finally add an entry to the changelog:

```
* **browser-engine-gecko**, **browser-engine-gecko-beta**, **browser-engine-gecko-nightly**
  * **Merge day!**
    * `browser-engine-gecko-release`: GeckoView 70.0
    * `browser-engine-gecko-beta`: GeckoView 71.0
    * `browser-engine-gecko-nightly`: GeckoView 72.0
```

### Build failures

* ***Gradle***: A common reason for build failures is changes in the Gradle configuration or the list of dependencies. Since we do not copy `build.gradle` (because it contains channel specific configuration) we need to manually compare the files to see if there's something to change.

* ***Breaking API changes***: Especially when updating the Nighlty component it can happen that there are breaking API changes that landed in GeckoView just right after the new version was started. So the first time we see them is after merge day when updating the major version of the dependency. In this case we need to fix those breaking changes manually like other breaking changes that happen in "normal" development phases.

