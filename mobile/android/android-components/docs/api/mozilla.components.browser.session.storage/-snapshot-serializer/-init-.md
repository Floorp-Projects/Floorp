[android-components](../../index.md) / [mozilla.components.browser.session.storage](../index.md) / [SnapshotSerializer](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`SnapshotSerializer(restoreSessionIds: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, restoreParentIds: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true)`

Helper to transform [SessionManager.Snapshot](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md) instances to JSON and back.

### Parameters

`restoreSessionIds` - If true the original [Session.id](../../mozilla.components.browser.session/-session/id.md) of [Session](../../mozilla.components.browser.session/-session/index.md)s will be restored. Otherwise a new ID will be
generated. An app may prefer to use new IDs if it expects sessions to get restored multiple times - otherwise
breaking the promise of a unique ID.

`restoreParentIds` - If true the [Session.parentId](#) will be restored, otherwise it will remain null. Setting
this to false is useful for features that do not have or rely on a parent/child relationship between tabs, esp.
if it can't be guaranteed that the parent tab is still available when the child tabs are restored.