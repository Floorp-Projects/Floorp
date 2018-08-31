---
title: Fretboard.withExperiment - 
---

[mozilla.components.service.fretboard](../index.html) / [Fretboard](index.html) / [withExperiment](./with-experiment.html)

# withExperiment

`fun withExperiment(context: Context, descriptor: `[`ExperimentDescriptor`](../-experiment-descriptor/index.html)`, block: (`[`Experiment`](../-experiment/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)

Performs an action if the user is part of the specified experiment

### Parameters

`context` - context

`descriptor` - descriptor of the experiment to check

`block` - block of code to be executed if the user is part of the experiment