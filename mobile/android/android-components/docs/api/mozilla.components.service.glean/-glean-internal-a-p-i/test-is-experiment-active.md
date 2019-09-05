[android-components](../../index.md) / [mozilla.components.service.glean](../index.md) / [GleanInternalAPI](index.md) / [testIsExperimentActive](./test-is-experiment-active.md)

# testIsExperimentActive

`fun testIsExperimentActive(experimentId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/Glean.kt#L289)

Tests whether an experiment is active, for testing purposes only.

### Parameters

`experimentId` - the id of the experiment to look for.

**Return**
true if the experiment is active and reported in pings, otherwise false

