# Substituting local ac and GV
To build fenix with a local android-components or local GeckoView, we recommend the following methods:

|type|fenix -> local ac|fenix -> local GV|fenix -> local ac -> local GV|
|-|-|-|-|
|**method**|local.properties|local.properties|manual publish + local.properties|

For instructions with `local.properties`, see [the root README](https://github.com/mozilla-mobile/fenix/blob/main/README.md). For instructions on manual publish + local.properties, keep reading. See [ac#8386](https://github.com/mozilla-mobile/android-components/issues/8386) for why we can't use local properties for fenix -> local ac -> local GV publishing.

## fenix -> local ac -> local GV
We're going to manually publish our GeckoView to our local maven repository, modify ac locally to consume it, and use local.properties to build fenix -> local ac.

### 1. Synchronize checkouts
To avoid breaking changes causing our builds to fail, we should make sure each of the repositories is on a commit from around the same time frame. You can use the [`android-components/tools/list_compatible_dependency_versions.py` script](https://github.com/mozilla-mobile/android-components/blob/main/tools/list_compatible_dependency_versions.py) to trivially identify ac and GV builds from a given fenix commit. If you want to synchronize builds from a GV or ac commit, you'll likely need to try to align around the commit merge dates (use `git log --pretty=fuller`).

### 2. Manually publish GeckoView
With our builds synchronized, we can publish our local changes to GeckoView. To publish GeckoView, run:
```sh
./mach build && ./mach gradle \
    geckoview:publishWithGeckoBinariesDebugPublicationToMavenLocal \
    exoplayer2:publishDebugPublicationToMavenLocal
```

This **needs to be run every time you make changes.**

We also need to know what version of GeckoView we published. You can make this local change:
```diff
diff --git a/mobile/android/geckoview/build.gradle b/mobile/android/geckoview/build.gradle
--- a/mobile/android/geckoview/build.gradle
+++ b/mobile/android/geckoview/build.gradle
@@ -382,16 +382,17 @@ android.libraryVariants.all { variant ->
     // and we can simply extend its inputs.  See
     // https://android.googlesource.com/platform/tools/base/+/0cbe8846f7d02c0bb6f07156b9f4fde16d96d329/build-system/gradle-core/src/main/java/com/android/build/gradle/tasks/BundleAar.kt#94.
     variant.packageLibraryProvider.get().from("${topsrcdir}/toolkit/components/telemetry/geckoview/streaming/metrics.yaml")
 }

 apply plugin: 'maven-publish'

 version = getVersionNumber()
+println("version = " + version)
 group = 'org.mozilla.geckoview'

 def getArtifactId() {
     def id = "geckoview" + project.ext.artifactSuffix

     if (!mozconfig.substs.MOZ_ANDROID_GECKOVIEW_LITE) {
         id += "-omni"
     }
```

And execute `./mach build | grep version` to find a version number like `98.0.20211208151112-SNAPSHOT`.

### 3. Modify ac to consume local GV
Update the build.gradle and Gecko.kt file in Fenix (see the diff below). Remember to update the GV version with the version you found in step 2!
```diff
diff --git a/build.gradle b/build.gradle
index fa8149781f..863df65a57 100644
--- a/build.gradle
+++ b/build.gradle
@@ -6,6 +6,7 @@ import static org.gradle.api.tasks.testing.TestResult.ResultType

 buildscript {
     repositories {
+        mavenLocal()
         if (project.hasProperty("googleRepo")) {
             maven {
                 name "Google"
@@ -40,6 +41,7 @@ plugins {

 allprojects {
     repositories {
+        mavenLocal()
         if (project.hasProperty("googleRepo")) {
             maven {
                 name "Google"
diff --git a/buildSrc/src/main/java/Gecko.kt b/buildSrc/src/main/java/Gecko.kt
index 331158bf50..f37a05791a 100644
--- a/buildSrc/src/main/java/Gecko.kt
+++ b/buildSrc/src/main/java/Gecko.kt
@@ -9,7 +9,10 @@ object Gecko {
     /**
      * GeckoView Version.
      */
-    const val version = "98.0.20220125100058"
+    const val version = "98.0.20211208151112-SNAPSHOT"

     /**
      * GeckoView channel
@@ -23,7 +26,7 @@ object Gecko {
 enum class GeckoChannel(
     val artifactName: String
 ) {
-    NIGHTLY("geckoview-nightly-omni"),
+    NIGHTLY("geckoview-default-omni"),
     BETA("geckoview-beta-omni"),
     RELEASE("geckoview-omni")
 }
```

### 4. Build fenix with local.properties change
Now build fenix as usual with [the `local.properties` change](https://github.com/mozilla-mobile/fenix#auto-publication-workflow-for-android-components-and-application-services) to build with your local ac. This build will automatically build ac each time it is run. You should have a fenix -> local ac -> local GV build now!
