---
title: ExperimentSource.getExperiments - 
---

[mozilla.components.service.fretboard](../index.html) / [ExperimentSource](index.html) / [getExperiments](./get-experiments.html)

# getExperiments

`abstract fun getExperiments(snapshot: `[`ExperimentsSnapshot`](../-experiments-snapshot/index.html)`): `[`ExperimentsSnapshot`](../-experiments-snapshot/index.html)

Requests new experiments from the source,
parsing the response into experiments

### Parameters

`client` - Http client to use, provided by Fretboard

`snapshot` - list of already downloaded experiments
(in order to process a diff response, for example)

**Return**
modified list of experiments

