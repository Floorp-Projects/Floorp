[android-components](../../index.md) / [mozilla.components.browser.session.storage](../index.md) / [SnapshotSerializer](./index.md)

# SnapshotSerializer

`class SnapshotSerializer` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/storage/SnapshotSerializer.kt#L21)

Helper to transform [SessionManager.Snapshot](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md) instances to JSON and back.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SnapshotSerializer()`<br>Helper to transform [SessionManager.Snapshot](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md) instances to JSON and back. |

### Functions

| Name | Summary |
|---|---|
| [fromJSON](from-j-s-o-n.md) | `fun fromJSON(engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, json: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Snapshot`](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md) |
| [itemFromJSON](item-from-j-s-o-n.md) | `fun itemFromJSON(engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, json: `[`JSONObject`](https://developer.android.com/reference/org/json/JSONObject.html)`): `[`Item`](../../mozilla.components.browser.session/-session-manager/-snapshot/-item/index.md) |
| [itemToJSON](item-to-j-s-o-n.md) | `fun itemToJSON(item: `[`Item`](../../mozilla.components.browser.session/-session-manager/-snapshot/-item/index.md)`): `[`JSONObject`](https://developer.android.com/reference/org/json/JSONObject.html) |
| [toJSON](to-j-s-o-n.md) | `fun toJSON(snapshot: `[`Snapshot`](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
