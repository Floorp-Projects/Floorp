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
| [equals](equals.md) | `fun equals(other: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [hashCode](hash-code.md) | `fun hashCode(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [toJSON](to-j-s-o-n.md) | `fun toJSON(): <ERROR CLASS>`<br>Create a JSON representation of this state that can be saved to disk. |

### Companion Object Functions

| Name | Summary |
|---|---|
| [fromJSON](from-j-s-o-n.md) | `fun fromJSON(json: <ERROR CLASS>): `[`ServoEngineSessionState`](./index.md) |
