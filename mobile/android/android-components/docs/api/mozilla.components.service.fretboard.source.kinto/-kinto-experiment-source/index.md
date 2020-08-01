[android-components](../../index.md) / [mozilla.components.service.fretboard.source.kinto](../index.md) / [KintoExperimentSource](./index.md)

# KintoExperimentSource

`class KintoExperimentSource : `[`ExperimentSource`](../../mozilla.components.service.fretboard/-experiment-source/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/fretboard/src/main/java/mozilla/components/service/fretboard/source/kinto/KintoExperimentSource.kt#L26)

Class responsible for fetching and
parsing experiments from a Kinto server

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `KintoExperimentSource(baseUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, bucketName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, collectionName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, httpClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, validateSignature: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`<br>Class responsible for fetching and parsing experiments from a Kinto server |

### Functions

| Name | Summary |
|---|---|
| [getExperiments](get-experiments.md) | `fun getExperiments(snapshot: `[`ExperimentsSnapshot`](../../mozilla.components.service.fretboard/-experiments-snapshot/index.md)`): `[`ExperimentsSnapshot`](../../mozilla.components.service.fretboard/-experiments-snapshot/index.md)<br>Requests new experiments from the source, parsing the response into experiments |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
