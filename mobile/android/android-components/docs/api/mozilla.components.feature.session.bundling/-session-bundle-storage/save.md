[android-components](../../index.md) / [mozilla.components.feature.session.bundling](../index.md) / [SessionBundleStorage](index.md) / [save](./save.md)

# save

`@Synchronized @WorkerThread fun save(snapshot: `[`Snapshot`](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session-bundling/src/main/java/mozilla/components/feature/session/bundling/SessionBundleStorage.kt#L71)

Overrides [Storage.save](../../mozilla.components.browser.session.storage/-auto-save/-storage/save.md)

Saves the [SessionManager.Snapshot](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md) as a bundle. If a bundle was restored previously then this bundle will be
updated with the data from the snapshot. If no bundle was restored a new bundle will be created.

