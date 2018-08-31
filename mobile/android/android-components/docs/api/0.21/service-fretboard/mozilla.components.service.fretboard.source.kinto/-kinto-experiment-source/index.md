---
title: KintoExperimentSource - 
---

[mozilla.components.service.fretboard.source.kinto](../index.html) / [KintoExperimentSource](./index.html)

# KintoExperimentSource

`class KintoExperimentSource : `[`ExperimentSource`](../../mozilla.components.service.fretboard/-experiment-source/index.html)

Class responsible for fetching and
parsing experiments from a Kinto server

### Constructors

| [&lt;init&gt;](-init-.html) | `KintoExperimentSource(baseUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, bucketName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, collectionName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, validateSignature: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, client: `[`HttpClient`](../-http-client/index.html)` = HttpURLConnectionHttpClient())`<br>Class responsible for fetching and parsing experiments from a Kinto server |

### Functions

| [getExperiments](get-experiments.html) | `fun getExperiments(snapshot: `[`ExperimentsSnapshot`](../../mozilla.components.service.fretboard/-experiments-snapshot/index.html)`): `[`ExperimentsSnapshot`](../../mozilla.components.service.fretboard/-experiments-snapshot/index.html)<br>Requests new experiments from the source, parsing the response into experiments |

