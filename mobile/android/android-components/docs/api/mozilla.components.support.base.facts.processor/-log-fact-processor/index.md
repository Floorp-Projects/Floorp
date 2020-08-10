[android-components](../../index.md) / [mozilla.components.support.base.facts.processor](../index.md) / [LogFactProcessor](./index.md)

# LogFactProcessor

`class LogFactProcessor : `[`FactProcessor`](../../mozilla.components.support.base.facts/-fact-processor/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/facts/processor/LogFactProcessor.kt#L14)

A [FactProcessor](../../mozilla.components.support.base.facts/-fact-processor/index.md) implementation that prints collected [Fact](../../mozilla.components.support.base.facts/-fact/index.md) instances to the log.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `LogFactProcessor(logger: `[`Logger`](../../mozilla.components.support.base.log.logger/-logger/index.md)` = Logger("Facts"))`<br>A [FactProcessor](../../mozilla.components.support.base.facts/-fact-processor/index.md) implementation that prints collected [Fact](../../mozilla.components.support.base.facts/-fact/index.md) instances to the log. |

### Functions

| Name | Summary |
|---|---|
| [process](process.md) | `fun process(fact: `[`Fact`](../../mozilla.components.support.base.facts/-fact/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Passes the given [Fact](../../mozilla.components.support.base.facts/-fact/index.md) to the [FactProcessor](../../mozilla.components.support.base.facts/-fact-processor/index.md) for processing. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
| [register](../../mozilla.components.support.base.facts/register.md) | `fun `[`FactProcessor`](../../mozilla.components.support.base.facts/-fact-processor/index.md)`.register(): `[`Facts`](../../mozilla.components.support.base.facts/-facts/index.md)<br>Registers this [FactProcessor](../../mozilla.components.support.base.facts/-fact-processor/index.md) to collect [Fact](../../mozilla.components.support.base.facts/-fact/index.md) instances from the [Facts](../../mozilla.components.support.base.facts/-facts/index.md) singleton. |
