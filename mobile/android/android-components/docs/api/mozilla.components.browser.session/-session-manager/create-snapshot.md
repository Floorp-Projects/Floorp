[android-components](../../index.md) / [mozilla.components.browser.session](../index.md) / [SessionManager](index.md) / [createSnapshot](./create-snapshot.md)

# createSnapshot

`fun createSnapshot(): `[`Snapshot`](-snapshot/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/SessionManager.kt#L81)

Produces a snapshot of this manager's state, suitable for restoring via [SessionManager.restore](restore.md).
Only regular sessions are included in the snapshot. Private and Custom Tab sessions are omitted.

**Return**
[Snapshot](-snapshot/index.md) of the current session state.

