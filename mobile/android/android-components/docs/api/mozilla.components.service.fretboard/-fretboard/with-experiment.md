[android-components](../../index.md) / [mozilla.components.service.fretboard](../index.md) / [Fretboard](index.md) / [withExperiment](./with-experiment.md)

# withExperiment

`fun withExperiment(context: <ERROR CLASS>, descriptor: `[`ExperimentDescriptor`](../-experiment-descriptor/index.md)`, block: (`[`Experiment`](../-experiment/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/fretboard/src/main/java/mozilla/components/service/fretboard/Fretboard.kt#L84)

Performs an action if the user is part of the specified experiment

### Parameters

`context` - context

`descriptor` - descriptor of the experiment to check

`block` - block of code to be executed if the user is part of the experiment