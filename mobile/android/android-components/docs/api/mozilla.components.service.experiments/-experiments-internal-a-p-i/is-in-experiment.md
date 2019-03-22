[android-components](../../index.md) / [mozilla.components.service.experiments](../index.md) / [ExperimentsInternalAPI](index.md) / [isInExperiment](./is-in-experiment.md)

# isInExperiment

`fun isInExperiment(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, experimentId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/experiments/src/main/java/mozilla/components/service/experiments/Experiments.kt#L147)

Checks if the user is part of
the specified experiment

### Parameters

`context` - context

`experimentId` - the id of the experiment

**Return**
true if the user is part of the specified experiment, false otherwise

