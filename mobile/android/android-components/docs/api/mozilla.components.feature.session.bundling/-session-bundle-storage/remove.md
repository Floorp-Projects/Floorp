[android-components](../../index.md) / [mozilla.components.feature.session.bundling](../index.md) / [SessionBundleStorage](index.md) / [remove](./remove.md)

# remove

`@Synchronized @WorkerThread fun remove(bundle: `[`SessionBundle`](../-session-bundle/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session-bundling/src/main/java/mozilla/components/feature/session/bundling/SessionBundleStorage.kt#L89)

Removes the given [SessionBundle](../-session-bundle/index.md) permanently. If this is the active bundle then a new one will be created the
next time a [SessionManager.Snapshot](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md) is saved.

