---
title: KintoExperimentSource.getExperiments - 
---

[mozilla.components.service.fretboard.source.kinto](../index.html) / [KintoExperimentSource](index.html) / [getExperiments](./get-experiments.html)

# getExperiments

`fun getExperiments(snapshot: `[`ExperimentsSnapshot`](../../mozilla.components.service.fretboard/-experiments-snapshot/index.html)`): `[`ExperimentsSnapshot`](../../mozilla.components.service.fretboard/-experiments-snapshot/index.html)

Overrides [ExperimentSource.getExperiments](../../mozilla.components.service.fretboard/-experiment-source/get-experiments.html)

Requests new experiments from the source,
parsing the response into experiments

### Parameters

`client` - Http client to use, provided by Fretboard

`snapshot` - list of already downloaded experiments
(in order to process a diff response, for example)

**Return**
modified list of experiments

