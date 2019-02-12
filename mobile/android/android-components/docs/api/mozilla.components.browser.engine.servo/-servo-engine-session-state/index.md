[android-components](../../index.md) / [mozilla.components.browser.engine.servo](../index.md) / [ServoEngineSessionState](./index.md)

# ServoEngineSessionState

`class ServoEngineSessionState : `[`EngineSessionState`](../../mozilla.components.concept.engine/-engine-session-state/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-servo/src/main/java/mozilla/components/browser/engine/servo/ServoEngineSessionState.kt#L13)

No-op implementation of [EngineSessionState](../../mozilla.components.concept.engine/-engine-session-state/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ServoEngineSessionState()`<br>No-op implementation of [EngineSessionState](../../mozilla.components.concept.engine/-engine-session-state/index.md). |

### Functions

| Name | Summary |
|---|---|
| [toJSON](to-j-s-o-n.md) | `fun toJSON(): `[`JSONObject`](https://developer.android.com/reference/org/json/JSONObject.html)<br>Create a JSON representation of this state that can be saved to disk. |

### Companion Object Functions

| Name | Summary |
|---|---|
| [fromJSON](from-j-s-o-n.md) | `fun fromJSON(json: `[`JSONObject`](https://developer.android.com/reference/org/json/JSONObject.html)`): `[`ServoEngineSessionState`](./index.md) |
