---
title: FlatFileExperimentStorage - 
---

[mozilla.components.service.fretboard.storage.flatfile](../index.html) / [FlatFileExperimentStorage](./index.html)

# FlatFileExperimentStorage

`class FlatFileExperimentStorage : `[`ExperimentStorage`](../../mozilla.components.service.fretboard/-experiment-storage/index.html)

Class which uses a flat JSON file as an experiment storage mechanism

### Parameters

`file` - file where to store experiments

### Constructors

| [&lt;init&gt;](-init-.html) | `FlatFileExperimentStorage(file: `[`File`](http://docs.oracle.com/javase/6/docs/api/java/io/File.html)`)`<br>Class which uses a flat JSON file as an experiment storage mechanism |

### Functions

| [retrieve](retrieve.html) | `fun retrieve(): `[`ExperimentsSnapshot`](../../mozilla.components.service.fretboard/-experiments-snapshot/index.html)<br>Reads experiments from disk |
| [save](save.html) | `fun save(snapshot: `[`ExperimentsSnapshot`](../../mozilla.components.service.fretboard/-experiments-snapshot/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stores the given experiments to disk |

