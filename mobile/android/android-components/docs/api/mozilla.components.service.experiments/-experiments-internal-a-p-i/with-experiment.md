[android-components](../../index.md) / [mozilla.components.service.experiments](../index.md) / [ExperimentsInternalAPI](index.md) / [withExperiment](./with-experiment.md)

# withExperiment

`fun withExperiment(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, experimentId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, block: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/experiments/src/main/java/mozilla/components/service/experiments/Experiments.kt#L158)

Performs an action if the user is part of the specified experiment

### Parameters

`context` - context

`experimentId` - the id of the experiment

`block` - block of code to be executed if the user is part of the experiment