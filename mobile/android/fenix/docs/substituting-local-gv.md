# Substituting local GeckoView

### 1. Manually publish local GeckoView to local Maven
First, to get the version that we are locally publishing (which will be used in later steps) please add the following:

*geckoview/build.gradle*
```diff
diff --git a/mobile/android/geckoview/build.gradle b/mobile/android/geckoview/build.gradle
index 731026796921b..81cc6782be291 100644
--- a/mobile/android/geckoview/build.gradle
+++ b/mobile/android/geckoview/build.gradle
@@ -398,6 +398,7 @@ android.libraryVariants.all { variant ->
 apply plugin: 'maven-publish'
 
 version = getVersionNumber()
+println("version = " + version)
 group = 'org.mozilla.geckoview'
 
 def getArtifactId() {

```

We can now publish our local GeckoView to our local maven.
```sh
./mach build && ./mach gradle \
    geckoview:publishWithGeckoBinariesDebugPublicationToMavenLocal \
    exoplayer2:publishDebugPublicationToMavenLocal
```

:warning: **This needs to be run every time you make changes.** :warning:

You need to copy the version in the logs or run 
```sh
./mach build | grep version
``` 
(ex. `115.0.20230511122045-SNAPSHOT`)

### 2. Modify Fenix to consume local GV
Update the build.gradle and Gecko.kt file in Fenix (see the diffs below). Remember to update the GV version with the version you found in step 2!

*fenix/build.gradle*
```diff 
diff --git a/fenix/build.gradle b/fenix/build.gradle
index 6a635a4818..4c8cc28995 100644
--- a/fenix/build.gradle
+++ b/fenix/build.gradle
@@ -5,6 +5,7 @@ import org.mozilla.fenix.gradle.tasks.GithubDetailsTask
 buildscript {
     // This logic is duplicated in the allprojects block: I don't know how to fix that.
     repositories {
+        mavenLocal()
         maven {
             name "Mozilla Nightly"
             url "https://nightly.maven.mozilla.org/maven2"
@@ -90,6 +91,7 @@ plugins {
 allprojects {
     // This logic is duplicated in the buildscript block: I don't know how to fix that.
     repositories {
+        mavenLocal()
         maven {
             name "Mozilla Nightly"
             url "https://nightly.maven.mozilla.org/maven2"
```
*Gecko.kt*
```diff
diff --git a/android-components/plugins/dependencies/src/main/java/Gecko.kt b/android-components/plugins/dependencies/src/main/java/Gecko.kt
index bed3fb0161..2d3a19a96e 100644
--- a/android-components/plugins/dependencies/src/main/java/Gecko.kt
+++ b/android-components/plugins/dependencies/src/main/java/Gecko.kt
@@ -9,7 +9,7 @@ object Gecko {
     /**
      * GeckoView Version.
      */
-    const val version = "115.0.20230511131014"
+    const val version = "115.0.20230511122045-SNAPSHOT"
 
     /**
      * GeckoView channel
@@ -23,7 +23,7 @@ object Gecko {
 enum class GeckoChannel(
     val artifactName: String,
 ) {
-    NIGHTLY("geckoview-nightly-omni"),
+    NIGHTLY("geckoview-default-omni"),
     BETA("geckoview-beta-omni"),
     RELEASE("geckoview-omni"),
 }

```

### 3. Build fenix with local GV
Now sync your gradle changes and build! 

An easy way to confirm you are using a local GV is switching your Android Studio project tool window to "Project" and looking in the root directory called "External Libraries" for "GeckoView". You should see something like `Gradle: org.mozilla.geckoview-default-omni:115.0.20230511122045-SNAPSHOT@aar`
