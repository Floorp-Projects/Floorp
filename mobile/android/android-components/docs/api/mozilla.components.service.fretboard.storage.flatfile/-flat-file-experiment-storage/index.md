[android-components](../../index.md) / [mozilla.components.service.fretboard.storage.flatfile](../index.md) / [FlatFileExperimentStorage](./index.md)

# FlatFileExperimentStorage

`class FlatFileExperimentStorage : `[`ExperimentStorage`](../../mozilla.components.service.fretboard/-experiment-storage/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/fretboard/src/main/java/mozilla/components/service/fretboard/storage/flatfile/FlatFileExperimentStorage.kt#L20)

Class which uses a flat JSON file as an experiment storage mechanism

### Parameters

`file` - file where to store experiments

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FlatFileExperimentStorage(file: `[`File`](http://docs.oracle.com/javase/7/docs/api/java/io/File.html)`)`<br>Class which uses a flat JSON file as an experiment storage mechanism |

### Functions

| Name | Summary |
|---|---|
| [retrieve](retrieve.md) | `fun retrieve(): `[`ExperimentsSnapshot`](../../mozilla.components.service.fretboard/-experiments-snapshot/index.md)<br>Reads experiments from disk |
| [save](save.md) | `fun save(snapshot: `[`ExperimentsSnapshot`](../../mozilla.components.service.fretboard/-experiments-snapshot/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stores the given experiments to disk |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
