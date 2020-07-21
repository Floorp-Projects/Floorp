[android-components](../index.md) / [mozilla.components.browser.session.ext](index.md) / [readSnapshotItem](./read-snapshot-item.md)

# readSnapshotItem

`fun <ERROR CLASS>.readSnapshotItem(engine: `[`Engine`](../mozilla.components.concept.engine/-engine/index.md)`, restoreSessionId: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, restoreParentId: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, serializer: `[`SnapshotSerializer`](../mozilla.components.browser.session.storage/-snapshot-serializer/index.md)` = SnapshotSerializer(restoreSessionId, restoreParentId)): `[`Item`](../mozilla.components.browser.session/-session-manager/-snapshot/-item/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/ext/AtomicFile.kt#L47)

Reads a single [SessionManager.Snapshot.Item](../mozilla.components.browser.session/-session-manager/-snapshot/-item/index.md) from this [AtomicFile](#). Returns `null` if no snapshot item could be
read.

