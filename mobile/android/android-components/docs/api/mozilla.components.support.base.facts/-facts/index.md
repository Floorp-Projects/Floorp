[android-components](../../index.md) / [mozilla.components.support.base.facts](../index.md) / [Facts](./index.md)

# Facts

`object Facts` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/facts/Facts.kt#L12)

Global API for collecting [Fact](../-fact/index.md) objects and forwarding them to [FactProcessor](../-fact-processor/index.md) instances.

### Functions

| Name | Summary |
|---|---|
| [clearProcessors](clear-processors.md) | `fun clearProcessors(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [collect](collect.md) | `fun collect(fact: `[`Fact`](../-fact/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Collects a [Fact](../-fact/index.md) and forwards it to all registered [FactProcessor](../-fact-processor/index.md) instances. |
| [registerProcessor](register-processor.md) | `fun registerProcessor(processor: `[`FactProcessor`](../-fact-processor/index.md)`): `[`Facts`](./index.md)<br>Registers a new [FactProcessor](../-fact-processor/index.md). |
