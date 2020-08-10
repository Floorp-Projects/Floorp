[android-components](../../index.md) / [mozilla.components.service.fretboard](../index.md) / [ExperimentSource](./index.md)

# ExperimentSource

`interface ExperimentSource` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/fretboard/src/main/java/mozilla/components/service/fretboard/ExperimentSource.kt#L11)

Represents a location where experiments are stored
(Kinto, a JSON file on a server, etc)

### Functions

| Name | Summary |
|---|---|
| [getExperiments](get-experiments.md) | `abstract fun getExperiments(snapshot: `[`ExperimentsSnapshot`](../-experiments-snapshot/index.md)`): `[`ExperimentsSnapshot`](../-experiments-snapshot/index.md)<br>Requests new experiments from the source, parsing the response into experiments |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [KintoExperimentSource](../../mozilla.components.service.fretboard.source.kinto/-kinto-experiment-source/index.md) | `class KintoExperimentSource : `[`ExperimentSource`](./index.md)<br>Class responsible for fetching and parsing experiments from a Kinto server |
