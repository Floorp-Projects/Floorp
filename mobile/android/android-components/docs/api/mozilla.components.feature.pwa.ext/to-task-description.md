[android-components](../index.md) / [mozilla.components.feature.pwa.ext](index.md) / [toTaskDescription](./to-task-description.md)

# toTaskDescription

`fun `[`WebAppManifest`](../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`.toTaskDescription(icon: <ERROR CLASS>?): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/ext/WebAppManifest.kt#L42)

Creates a [TaskDescription](#) for the activity manager based on the manifest.

Since the web app icon is provided dynamically by the web site, we can't provide a resource ID.
Instead we use the deprecated constructor.

