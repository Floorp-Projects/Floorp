---
title: ExperimentSource - 
---

[mozilla.components.service.fretboard](../index.html) / [ExperimentSource](./index.html)

# ExperimentSource

`interface ExperimentSource`

Represents a location where experiments are stored
(Kinto, a JSON file on a server, etc)

### Functions

| [getExperiments](get-experiments.html) | `abstract fun getExperiments(snapshot: `[`ExperimentsSnapshot`](../-experiments-snapshot/index.html)`): `[`ExperimentsSnapshot`](../-experiments-snapshot/index.html)<br>Requests new experiments from the source, parsing the response into experiments |

### Inheritors

| [KintoExperimentSource](../../mozilla.components.service.fretboard.source.kinto/-kinto-experiment-source/index.html) | `class KintoExperimentSource : `[`ExperimentSource`](./index.md)<br>Class responsible for fetching and parsing experiments from a Kinto server |

