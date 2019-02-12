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

### Inheritors

| Name | Summary |
|---|---|
| [FlatFileExperimentStorage](../../mozilla.components.service.fretboard.storage.flatfile/-flat-file-experiment-storage/index.md) | `class FlatFileExperimentStorage : `[`ExperimentStorage`](./index.md)<br>Class which uses a flat JSON file as an experiment storage mechanism |
