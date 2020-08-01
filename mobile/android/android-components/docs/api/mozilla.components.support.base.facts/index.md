[android-components](../index.md) / [mozilla.components.support.base.facts](./index.md)

## Package mozilla.components.support.base.facts

### Types

| Name | Summary |
|---|---|
| [Action](-action/index.md) | `enum class Action`<br>A user or system action that causes [Fact](-fact/index.md) instances to be emitted. |
| [Fact](-fact/index.md) | `data class Fact`<br>A fact describing a generic event that has occurred in a component. |
| [FactProcessor](-fact-processor/index.md) | `interface FactProcessor`<br>A [FactProcessor](-fact-processor/index.md) receives [Fact](-fact/index.md) instances to process them further. |
| [Facts](-facts/index.md) | `object Facts`<br>Global API for collecting [Fact](-fact/index.md) objects and forwarding them to [FactProcessor](-fact-processor/index.md) instances. |

### Functions

| Name | Summary |
|---|---|
| [collect](collect.md) | `fun `[`Fact`](-fact/index.md)`.collect(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Collect this fact through the [Facts](-facts/index.md) singleton. |
| [register](register.md) | `fun `[`FactProcessor`](-fact-processor/index.md)`.register(): `[`Facts`](-facts/index.md)<br>Registers this [FactProcessor](-fact-processor/index.md) to collect [Fact](-fact/index.md) instances from the [Facts](-facts/index.md) singleton. |
