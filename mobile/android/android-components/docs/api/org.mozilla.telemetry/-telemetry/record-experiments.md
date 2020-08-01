[android-components](../../index.md) / [org.mozilla.telemetry](../index.md) / [Telemetry](index.md) / [recordExperiments](./record-experiments.md)

# recordExperiments

`open fun recordExperiments(experiments: `[`MutableMap`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-mutable-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>): `[`Telemetry`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/Telemetry.java#L253)

Records all experiments the client knows of in the event ping.

### Parameters

`experiments` - A map of experiments the client knows of. Mapping experiment name to a Boolean value that is true if the client is part of the experiment and false if the client is not part of the experiment.