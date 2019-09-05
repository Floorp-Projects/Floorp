[android-components](../index.md) / [mozilla.components.service.experiments](index.md) / [Experiments](./-experiments.md)

# Experiments

`object Experiments : `[`ExperimentsInternalAPI`](-experiments-internal-a-p-i/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/experiments/src/main/java/mozilla/components/service/experiments/Experiments.kt#L399)

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
| [initialize](-experiments-internal-a-p-i/initialize.md) | `fun initialize(applicationContext: <ERROR CLASS>, configuration: `[`Configuration`](-configuration/index.md)` = Configuration()): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Initialize the experiments library. |
| [withExperiment](-experiments-internal-a-p-i/with-experiment.md) | `fun withExperiment(experimentId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, block: (branch: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Performs an action if the user is part of the specified experiment |
