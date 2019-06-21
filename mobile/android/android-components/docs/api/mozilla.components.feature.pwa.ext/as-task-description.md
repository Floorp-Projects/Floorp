[android-components](../index.md) / [mozilla.components.feature.pwa.ext](index.md) / [asTaskDescription](./as-task-description.md)

# asTaskDescription

`fun `[`WebAppManifest`](../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`.asTaskDescription(): `[`TaskDescription`](https://developer.android.com/reference/android/app/ActivityManager/TaskDescription.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/ext/WebAppManifest.kt#L17)

Create a [TaskDescription](https://developer.android.com/reference/android/app/ActivityManager/TaskDescription.html) for the activity manager based on the manifest.

Since the web app icon is provided dynamically by the web site, we can't provide a resource ID.
Instead we use the deprecated constructor.

