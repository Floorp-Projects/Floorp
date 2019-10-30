[android-components](../../index.md) / [mozilla.components.support.base.facts](../index.md) / [FactProcessor](./index.md)

# FactProcessor

`interface FactProcessor` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/facts/FactProcessor.kt#L10)

A [FactProcessor](./index.md) receives [Fact](../-fact/index.md) instances to process them further.

### Functions

| Name | Summary |
|---|---|
| [process](process.md) | `abstract fun process(fact: `[`Fact`](../-fact/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Passes the given [Fact](../-fact/index.md) to the [FactProcessor](./index.md) for processing. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
| [register](../register.md) | `fun `[`FactProcessor`](./index.md)`.register(): `[`Facts`](../-facts/index.md)<br>Registers this [FactProcessor](./index.md) to collect [Fact](../-fact/index.md) instances from the [Facts](../-facts/index.md) singleton. |

### Inheritors

| Name | Summary |
|---|---|
| [CollectionProcessor](../../mozilla.components.support.base.facts.processor/-collection-processor/index.md) | `class CollectionProcessor : `[`FactProcessor`](./index.md)<br>A [FactProcessor](./index.md) implementation that keeps all [Fact](../-fact/index.md) objects in a list. |
| [LogFactProcessor](../../mozilla.components.support.base.facts.processor/-log-fact-processor/index.md) | `class LogFactProcessor : `[`FactProcessor`](./index.md)<br>A [FactProcessor](./index.md) implementation that prints collected [Fact](../-fact/index.md) instances to the log. |
