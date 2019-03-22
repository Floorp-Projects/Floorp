[android-components](../../index.md) / [mozilla.components.service.experiments](../index.md) / [ExperimentsInternalAPI](./index.md)

# ExperimentsInternalAPI

`open class ExperimentsInternalAPI` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/experiments/src/main/java/mozilla/components/service/experiments/Experiments.kt#L22)

Entry point of the library

### Functions

| Name | Summary |
|---|---|
| [initialize](initialize.md) | `fun initialize(applicationContext: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, fetchClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)` = HttpURLConnectionClient()): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Initialize the experiments library. |
| [isInExperiment](is-in-experiment.md) | `fun isInExperiment(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, experimentId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks if the user is part of the specified experiment |
| [withExperiment](with-experiment.md) | `fun withExperiment(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, experimentId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, block: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Performs an action if the user is part of the specified experiment |

### Inheritors

| Name | Summary |
|---|---|
| [Experiments](../-experiments.md) | `object Experiments : `[`ExperimentsInternalAPI`](./index.md) |
