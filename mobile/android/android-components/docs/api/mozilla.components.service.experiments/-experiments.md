[android-components](../index.md) / [mozilla.components.service.experiments](index.md) / [Experiments](./-experiments.md)

# Experiments

`object Experiments : `[`ExperimentsInternalAPI`](-experiments-internal-a-p-i/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/experiments/src/main/java/mozilla/components/service/experiments/Experiments.kt#L434)

The main Experiments object.

This is a global object that must be initialized by the application by calling the [initialize](-experiments-internal-a-p-i/initialize.md)
function before the experiments library can fetch updates from the server or be used to determine
experiment enrollment.

```
Experiments.initialize(applicationContext)
```

### Inherited Functions

| Name | Summary |
|---|---|
| [initialize](-experiments-internal-a-p-i/initialize.md) | `fun initialize(applicationContext: <ERROR CLASS>, configuration: `[`Configuration`](-configuration/index.md)`, onExperimentsUpdated: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Initialize the experiments library. |
| [withExperiment](-experiments-internal-a-p-i/with-experiment.md) | `fun withExperiment(experimentId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, block: (branch: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Performs an action if the user is part of the specified experiment |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
