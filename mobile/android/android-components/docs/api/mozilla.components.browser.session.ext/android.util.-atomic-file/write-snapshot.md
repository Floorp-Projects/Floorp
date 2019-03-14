[android-components](../../index.md) / [mozilla.components.browser.session.ext](../index.md) / [android.util.AtomicFile](index.md) / [writeSnapshot](./write-snapshot.md)

# writeSnapshot

`fun `[`AtomicFile`](https://developer.android.com/reference/android/util/AtomicFile.html)`.writeSnapshot(snapshot: `[`Snapshot`](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md)`, serializer: `[`SnapshotSerializer`](../../mozilla.components.browser.session.storage/-snapshot-serializer/index.md)` = SnapshotSerializer()): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/ext/AtomicFile.kt#L44)

Saves the given [SessionManager.Snapshot](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md) to this [AtomicFile](https://developer.android.com/reference/android/util/AtomicFile.html).

