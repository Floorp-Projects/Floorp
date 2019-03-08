[android-components](../../index.md) / [mozilla.components.service.experiments.source.kinto](../index.md) / [KintoExperimentSource](index.md) / [getExperiments](./get-experiments.md)

# getExperiments

`fun getExperiments(snapshot: `[`ExperimentsSnapshot`](../../mozilla.components.service.experiments/-experiments-snapshot/index.md)`): `[`ExperimentsSnapshot`](../../mozilla.components.service.experiments/-experiments-snapshot/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/experiments/src/main/java/mozilla/components/service/experiments/source/kinto/KintoExperimentSource.kt#L36)

Overrides [ExperimentSource.getExperiments](../../mozilla.components.service.experiments/-experiment-source/get-experiments.md)

Requests new experiments from the source,
parsing the response into experiments

### Parameters

`client` - Http client to use, provided by Experiments

`snapshot` - list of already downloaded experiments
(in order to process a diff response, for example)

**Return**
modified list of experiments

