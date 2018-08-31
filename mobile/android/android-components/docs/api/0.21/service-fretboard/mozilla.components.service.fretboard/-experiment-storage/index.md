---
title: ExperimentStorage - 
---

[mozilla.components.service.fretboard](../index.html) / [ExperimentStorage](./index.html)

# ExperimentStorage

`interface ExperimentStorage`

Represents a location where experiments
are stored locally on the device

### Functions

| [retrieve](retrieve.html) | `abstract fun retrieve(): `[`ExperimentsSnapshot`](../-experiments-snapshot/index.html)<br>Reads experiments from disk |
| [save](save.html) | `abstract fun save(snapshot: `[`ExperimentsSnapshot`](../-experiments-snapshot/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stores the given experiments to disk |

### Inheritors

| [FlatFileExperimentStorage](../../mozilla.components.service.fretboard.storage.flatfile/-flat-file-experiment-storage/index.html) | `class FlatFileExperimentStorage : `[`ExperimentStorage`](./index.md)<br>Class which uses a flat JSON file as an experiment storage mechanism |

