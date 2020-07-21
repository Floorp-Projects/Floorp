[android-components](../../index.md) / [mozilla.components.browser.engine.gecko.glean](../index.md) / [GeckoAdapter](./index.md)

# GeckoAdapter

`class GeckoAdapter : `[`Delegate`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/RuntimeTelemetry/Delegate.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/glean/GeckoAdapter.kt#L21)

This implements a [RuntimeTelemetry.Delegate](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/RuntimeTelemetry/Delegate.html) that dispatches Gecko runtime
telemetry to the Glean SDK.

Metrics defined in the `metrics.yaml` file in Gecko's mozilla-central repository
will be automatically dispatched to the Glean SDK and sent through the requested
pings.

This can be used, in products collecting data through the Glean SDK, by
providing an instance to `GeckoRuntimeSettings.Builder().telemetryDelegate`.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `GeckoAdapter()`<br>This implements a [RuntimeTelemetry.Delegate](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/RuntimeTelemetry/Delegate.html) that dispatches Gecko runtime telemetry to the Glean SDK. |

### Functions

| Name | Summary |
|---|---|
| [onBooleanScalar](on-boolean-scalar.md) | `fun onBooleanScalar(metric: `[`Metric`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/RuntimeTelemetry/Metric.html)`<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onHistogram](on-histogram.md) | `fun onHistogram(metric: `[`Histogram`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/RuntimeTelemetry/Histogram.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onLongScalar](on-long-scalar.md) | `fun onLongScalar(metric: `[`Metric`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/RuntimeTelemetry/Metric.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onStringScalar](on-string-scalar.md) | `fun onStringScalar(metric: `[`Metric`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/RuntimeTelemetry/Metric.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
