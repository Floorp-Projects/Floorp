[android-components](../../index.md) / [mozilla.components.support.base.facts](../index.md) / [Fact](./index.md)

# Fact

`data class Fact` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/facts/Fact.kt#L18)

A fact describing a generic event that has occurred in a component.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Fact(component: <ERROR CLASS>, action: `[`Action`](../-action/index.md)`, item: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, value: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, metadata: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`>? = null)`<br>A fact describing a generic event that has occurred in a component. |

### Properties

| Name | Summary |
|---|---|
| [action](action.md) | `val action: `[`Action`](../-action/index.md)<br>A user or system action that caused this fact (e.g. Action.CLICK). |
| [component](component.md) | `val component: <ERROR CLASS>`<br>Component that emitted this fact. |
| [item](item.md) | `val item: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>An item that caused the action or that the action was performed on (e.g. "toolbar"). |
| [metadata](metadata.md) | `val metadata: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`>?`<br>A key/value map for facts where additional richer context is needed. |
| [value](value.md) | `val value: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>An optional value providing more context. |

### Extension Functions

| Name | Summary |
|---|---|
| [collect](../collect.md) | `fun `[`Fact`](./index.md)`.collect(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Collect this fact through the [Facts](../-facts/index.md) singleton. |
