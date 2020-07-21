[android-components](../index.md) / [mozilla.components.service.fretboard](./index.md)

## Package mozilla.components.service.fretboard

### Types

| Name | Summary |
|---|---|
| [Experiment](-experiment/index.md) | `data class Experiment`<br>Represents an A/B test experiment, independent of the underlying storage mechanism |
| [ExperimentDescriptor](-experiment-descriptor/index.md) | `data class ExperimentDescriptor`<br>Class used to identify an experiment |
| [ExperimentPayload](-experiment-payload/index.md) | `class ExperimentPayload`<br>Class which represents an experiment associated data |
| [ExperimentSource](-experiment-source/index.md) | `interface ExperimentSource`<br>Represents a location where experiments are stored (Kinto, a JSON file on a server, etc) |
| [ExperimentStorage](-experiment-storage/index.md) | `interface ExperimentStorage`<br>Represents a location where experiments are stored locally on the device |
| [ExperimentsSnapshot](-experiments-snapshot/index.md) | `data class ExperimentsSnapshot`<br>Represents an experiment sync result |
| [Fretboard](-fretboard/index.md) | `class Fretboard`<br>Entry point of the library |
| [JSONExperimentParser](-j-s-o-n-experiment-parser/index.md) | `class JSONExperimentParser`<br>Default JSON parsing implementation |
| [ValuesProvider](-values-provider/index.md) | `open class ValuesProvider`<br>Class used to provide custom filter values |

### Exceptions

| Name | Summary |
|---|---|
| [ExperimentDownloadException](-experiment-download-exception/index.md) | `class ExperimentDownloadException : `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)<br>Exception while downloading experiments from the server |
