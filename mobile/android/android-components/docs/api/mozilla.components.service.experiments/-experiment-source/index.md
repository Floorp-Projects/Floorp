[android-components](../../index.md) / [mozilla.components.service.experiments](../index.md) / [ExperimentSource](./index.md)

# ExperimentSource

`interface ExperimentSource` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/experiments/src/main/java/mozilla/components/service/experiments/ExperimentSource.kt#L11)

Represents a location where experiments are stored
(Kinto, a JSON file on a server, etc)

### Functions

| Name | Summary |
|---|---|
| [getExperiments](get-experiments.md) | `abstract fun getExperiments(snapshot: `[`ExperimentsSnapshot`](../-experiments-snapshot/index.md)`): `[`ExperimentsSnapshot`](../-experiments-snapshot/index.md)<br>Requests new experiments from the source, parsing the response into experiments |

### Inheritors

| Name | Summary |
|---|---|
| [KintoExperimentSource](../../mozilla.components.service.experiments.source.kinto/-kinto-experiment-source/index.md) | `class KintoExperimentSource : `[`ExperimentSource`](./index.md)<br>Class responsible for fetching and parsing experiments from a Kinto server |
