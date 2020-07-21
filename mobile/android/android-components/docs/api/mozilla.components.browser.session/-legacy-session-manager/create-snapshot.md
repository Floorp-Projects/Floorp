[android-components](../../index.md) / [mozilla.components.browser.session](../index.md) / [LegacySessionManager](index.md) / [createSnapshot](./create-snapshot.md)

# createSnapshot

`fun createSnapshot(): `[`Snapshot`](../-session-manager/-snapshot/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/LegacySessionManager.kt#L44)

Produces a snapshot of this manager's state, suitable for restoring via [SessionManager.restore](../-session-manager/restore.md).
Only regular sessions are included in the snapshot. Private and Custom Tab sessions are omitted.

**Return**
[SessionManager.Snapshot](../-session-manager/-snapshot/index.md) of the current session state.

