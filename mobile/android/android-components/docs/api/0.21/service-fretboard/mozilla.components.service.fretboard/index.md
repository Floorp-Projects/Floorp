---
title: mozilla.components.service.fretboard - 
---

[mozilla.components.service.fretboard](./index.html)

## Package mozilla.components.service.fretboard

### Types

| [Experiment](-experiment/index.html) | `data class Experiment`<br>Represents an A/B test experiment, independent of the underlying storage mechanism |
| [ExperimentDescriptor](-experiment-descriptor/index.html) | `data class ExperimentDescriptor`<br>Class used to identify an experiment |
| [ExperimentPayload](-experiment-payload/index.html) | `class ExperimentPayload`<br>Class which represents an experiment associated data |
| [ExperimentSource](-experiment-source/index.html) | `interface ExperimentSource`<br>Represents a location where experiments are stored (Kinto, a JSON file on a server, etc) |
| [ExperimentStorage](-experiment-storage/index.html) | `interface ExperimentStorage`<br>Represents a location where experiments are stored locally on the device |
| [ExperimentsSnapshot](-experiments-snapshot/index.html) | `data class ExperimentsSnapshot`<br>Represents an experiment sync result |
| [Fretboard](-fretboard/index.html) | `class Fretboard`<br>Entry point of the library |
| [JSONExperimentParser](-j-s-o-n-experiment-parser/index.html) | `class JSONExperimentParser`<br>Default JSON parsing implementation |
| [ValuesProvider](-values-provider/index.html) | `open class ValuesProvider`<br>Class used to provide custom filter values |

### Exceptions

| [ExperimentDownloadException](-experiment-download-exception/index.html) | `class ExperimentDownloadException : `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)<br>Exception while downloading experiments from the server |

