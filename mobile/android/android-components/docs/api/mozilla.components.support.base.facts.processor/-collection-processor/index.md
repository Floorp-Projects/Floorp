[android-components](../../index.md) / [mozilla.components.support.base.facts.processor](../index.md) / [CollectionProcessor](./index.md)

# CollectionProcessor

`class CollectionProcessor : `[`FactProcessor`](../../mozilla.components.support.base.facts/-fact-processor/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/facts/processor/CollectionProcessor.kt#L18)

A [FactProcessor](../../mozilla.components.support.base.facts/-fact-processor/index.md) implementation that keeps all [Fact](../../mozilla.components.support.base.facts/-fact/index.md) objects in a list.

This [FactProcessor](../../mozilla.components.support.base.facts/-fact-processor/index.md) is only for testing.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `CollectionProcessor()`<br>A [FactProcessor](../../mozilla.components.support.base.facts/-fact-processor/index.md) implementation that keeps all [Fact](../../mozilla.components.support.base.facts/-fact/index.md) objects in a list. |

### Properties

| Name | Summary |
|---|---|
| [facts](facts.md) | `val facts: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Fact`](../../mozilla.components.support.base.facts/-fact/index.md)`>` |

### Functions

| Name | Summary |
|---|---|
| [process](process.md) | `fun process(fact: `[`Fact`](../../mozilla.components.support.base.facts/-fact/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Passes the given [Fact](../../mozilla.components.support.base.facts/-fact/index.md) to the [FactProcessor](../../mozilla.components.support.base.facts/-fact-processor/index.md) for processing. |

### Companion Object Functions

| Name | Summary |
|---|---|
| [withFactCollection](with-fact-collection.md) | `fun withFactCollection(block: (`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Fact`](../../mozilla.components.support.base.facts/-fact/index.md)`>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Helper for creating a [CollectionProcessor](./index.md), registering it and clearing the processors again. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
| [register](../../mozilla.components.support.base.facts/register.md) | `fun `[`FactProcessor`](../../mozilla.components.support.base.facts/-fact-processor/index.md)`.register(): `[`Facts`](../../mozilla.components.support.base.facts/-facts/index.md)<br>Registers this [FactProcessor](../../mozilla.components.support.base.facts/-fact-processor/index.md) to collect [Fact](../../mozilla.components.support.base.facts/-fact/index.md) instances from the [Facts](../../mozilla.components.support.base.facts/-facts/index.md) singleton. |
