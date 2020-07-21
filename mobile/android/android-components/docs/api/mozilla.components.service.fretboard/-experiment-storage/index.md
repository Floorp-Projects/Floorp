[android-components](../../index.md) / [mozilla.components.service.fretboard](../index.md) / [ExperimentStorage](./index.md)

# ExperimentStorage

`interface ExperimentStorage` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/fretboard/src/main/java/mozilla/components/service/fretboard/ExperimentStorage.kt#L11)

Represents a location where experiments
are stored locally on the device

### Functions

| Name | Summary |
|---|---|
| [retrieve](retrieve.md) | `abstract fun retrieve(): `[`ExperimentsSnapshot`](../-experiments-snapshot/index.md)<br>Reads experiments from disk |
| [save](save.md) | `abstract fun save(snapshot: `[`ExperimentsSnapshot`](../-experiments-snapshot/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stores the given experiments to disk |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [FlatFileExperimentStorage](../../mozilla.components.service.fretboard.storage.flatfile/-flat-file-experiment-storage/index.md) | `class FlatFileExperimentStorage : `[`ExperimentStorage`](./index.md)<br>Class which uses a flat JSON file as an experiment storage mechanism |
