[android-components](../../index.md) / [mozilla.components.browser.session.storage](../index.md) / [SnapshotSerializer](./index.md)

# SnapshotSerializer

`class SnapshotSerializer` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/storage/SnapshotSerializer.kt#L32)

Helper to transform [SessionManager.Snapshot](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md) instances to JSON and back.

### Parameters

`restoreSessionIds` - If true the original [Session.id](../../mozilla.components.browser.session/-session/id.md) of [Session](../../mozilla.components.browser.session/-session/index.md)s will be restored. Otherwise a new ID will be
generated. An app may prefer to use new IDs if it expects sessions to get restored multiple times - otherwise
breaking the promise of a unique ID.

`restoreParentIds` - If true the [Session.parentId](#) will be restored, otherwise it will remain null. Setting
this to false is useful for features that do not have or rely on a parent/child relationship between tabs, esp.
if it can't be guaranteed that the parent tab is still available when the child tabs are restored.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SnapshotSerializer(restoreSessionIds: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, restoreParentIds: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true)`<br>Helper to transform [SessionManager.Snapshot](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md) instances to JSON and back. |

### Functions

| Name | Summary |
|---|---|
| [fromJSON](from-j-s-o-n.md) | `fun fromJSON(engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, json: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Snapshot`](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md) |
| [itemFromJSON](item-from-j-s-o-n.md) | `fun itemFromJSON(engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, json: <ERROR CLASS>): `[`Item`](../../mozilla.components.browser.session/-session-manager/-snapshot/-item/index.md) |
| [itemToJSON](item-to-j-s-o-n.md) | `fun itemToJSON(item: `[`Item`](../../mozilla.components.browser.session/-session-manager/-snapshot/-item/index.md)`): <ERROR CLASS>` |
| [toJSON](to-j-s-o-n.md) | `fun toJSON(snapshot: `[`Snapshot`](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
