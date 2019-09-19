[android-components](../../index.md) / [mozilla.components.service.experiments](../index.md) / [ExperimentsInternalAPI](./index.md)

# ExperimentsInternalAPI

`open class ExperimentsInternalAPI` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/experiments/src/main/java/mozilla/components/service/experiments/Experiments.kt#L18)

This is the main experiments API, which is exposed through the global [Experiments](../-experiments.md) object.

### Functions

| Name | Summary |
|---|---|
| [initialize](initialize.md) | `fun initialize(applicationContext: <ERROR CLASS>, configuration: `[`Configuration`](../-configuration/index.md)` = Configuration()): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Initialize the experiments library. |
| [withExperiment](with-experiment.md) | `fun withExperiment(experimentId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, block: (branch: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Performs an action if the user is part of the specified experiment |

### Inheritors

| Name | Summary |
|---|---|
| [Experiments](../-experiments.md) | `object Experiments : `[`ExperimentsInternalAPI`](./index.md)<br>The main Experiments object. |
