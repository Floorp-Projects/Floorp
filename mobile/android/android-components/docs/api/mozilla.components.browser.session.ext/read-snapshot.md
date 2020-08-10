[android-components](../index.md) / [mozilla.components.browser.session.ext](index.md) / [readSnapshot](./read-snapshot.md)

# readSnapshot

`fun <ERROR CLASS>.readSnapshot(engine: `[`Engine`](../mozilla.components.concept.engine/-engine/index.md)`, serializer: `[`SnapshotSerializer`](../mozilla.components.browser.session.storage/-snapshot-serializer/index.md)` = SnapshotSerializer()): `[`Snapshot`](../mozilla.components.browser.session/-session-manager/-snapshot/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/ext/AtomicFile.kt#L18)

Read a [SessionManager.Snapshot](../mozilla.components.browser.session/-session-manager/-snapshot/index.md) from this [AtomicFile](#). Returns `null` if no snapshot could be read.

