[android-components](../../index.md) / [mozilla.components.service.fretboard](../index.md) / [ExperimentSource](index.md) / [getExperiments](./get-experiments.md)

# getExperiments

`abstract fun getExperiments(snapshot: `[`ExperimentsSnapshot`](../-experiments-snapshot/index.md)`): `[`ExperimentsSnapshot`](../-experiments-snapshot/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/fretboard/src/main/java/mozilla/components/service/fretboard/ExperimentSource.kt#L21)

Requests new experiments from the source,
parsing the response into experiments

### Parameters

`client` - Http client to use, provided by Fretboard

`snapshot` - list of already downloaded experiments
(in order to process a diff response, for example)

**Return**
modified list of experiments

