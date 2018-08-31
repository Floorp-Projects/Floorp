---
title: Fretboard - 
---

[mozilla.components.service.fretboard](../index.html) / [Fretboard](./index.html)

# Fretboard

`class Fretboard`

Entry point of the library

### Constructors

| [&lt;init&gt;](-init-.html) | `Fretboard(source: `[`ExperimentSource`](../-experiment-source/index.html)`, storage: `[`ExperimentStorage`](../-experiment-storage/index.html)`, valuesProvider: `[`ValuesProvider`](../-values-provider/index.html)` = ValuesProvider())`<br>Entry point of the library |

### Properties

| [experiments](experiments.html) | `val experiments: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Experiment`](../-experiment/index.html)`>`<br>Provides the list of experiments (active or not) |

### Functions

| [clearAllOverrides](clear-all-overrides.html) | `fun clearAllOverrides(context: Context): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Clears all experiment overrides |
| [clearOverride](clear-override.html) | `fun clearOverride(context: Context, descriptor: `[`ExperimentDescriptor`](../-experiment-descriptor/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Clears an override for a specified experiment |
| [getActiveExperiments](get-active-experiments.html) | `fun getActiveExperiments(context: Context): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Experiment`](../-experiment/index.html)`>`<br>Provides the list of active experiments |
| [getExperiment](get-experiment.html) | `fun getExperiment(descriptor: `[`ExperimentDescriptor`](../-experiment-descriptor/index.html)`): `[`Experiment`](../-experiment/index.html)`?`<br>Gets the metadata associated with the specified experiment, even if the user is not part of it |
| [isInExperiment](is-in-experiment.html) | `fun isInExperiment(context: Context, descriptor: `[`ExperimentDescriptor`](../-experiment-descriptor/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks if the user is part of the specified experiment |
| [loadExperiments](load-experiments.html) | `fun loadExperiments(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Loads experiments from local storage |
| [setOverride](set-override.html) | `fun setOverride(context: Context, descriptor: `[`ExperimentDescriptor`](../-experiment-descriptor/index.html)`, active: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Overrides a specified experiment |
| [updateExperiments](update-experiments.html) | `fun updateExperiments(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Requests new experiments from the server and saves them to local storage |
| [withExperiment](with-experiment.html) | `fun withExperiment(context: Context, descriptor: `[`ExperimentDescriptor`](../-experiment-descriptor/index.html)`, block: (`[`Experiment`](../-experiment/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Performs an action if the user is part of the specified experiment |

