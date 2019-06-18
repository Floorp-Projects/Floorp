[android-components](../../index.md) / [mozilla.components.browser.session.ext](../index.md) / [android.util.AtomicFile](index.md) / [readSnapshotItem](./read-snapshot-item.md)

# readSnapshotItem

`fun `[`AtomicFile`](https://developer.android.com/reference/android/util/AtomicFile.html)`.readSnapshotItem(engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, restoreSessionId: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, serializer: `[`SnapshotSerializer`](../../mozilla.components.browser.session.storage/-snapshot-serializer/index.md)` = SnapshotSerializer(restoreSessionId)): `[`Item`](../../mozilla.components.browser.session/-session-manager/-snapshot/-item/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/ext/AtomicFile.kt#L48)

Reads a single [SessionManager.Snapshot.Item](../../mozilla.components.browser.session/-session-manager/-snapshot/-item/index.md) from this [AtomicFile](https://developer.android.com/reference/android/util/AtomicFile.html). Returns `null` if no snapshot item could be
read.

