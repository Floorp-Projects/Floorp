[android-components](../../index.md) / [mozilla.components.service.glean](../index.md) / [GleanInternalAPI](index.md) / [testGetExperimentData](./test-get-experiment-data.md)

# testGetExperimentData

`fun testGetExperimentData(experimentId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`RecordedExperimentData`](../../mozilla.components.service.glean.storages/-recorded-experiment-data/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/Glean.kt#L291)

Returns the stored data for the requested active experiment, for testing purposes only.

### Parameters

`experimentId` - the id of the experiment to look for.

### Exceptions

`NullPointerException` - if the requested experiment is not active

**Return**
the [RecordedExperimentData](../../mozilla.components.service.glean.storages/-recorded-experiment-data/index.md) for the experiment

