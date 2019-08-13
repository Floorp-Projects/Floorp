[android-components](../../index.md) / [mozilla.components.service.glean](../index.md) / [GleanInternalAPI](index.md) / [setExperimentActive](./set-experiment-active.md)

# setExperimentActive

`fun setExperimentActive(experimentId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, branch: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, extra: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/Glean.kt#L265)

Indicate that an experiment is running. Glean will then add an
experiment annotation to the environment which is sent with pings. This
information is not persisted between runs.

### Parameters

`experimentId` - The id of the active experiment (maximum
    30 bytes)

`branch` - The experiment branch (maximum 30 bytes)

`extra` - Optional metadata to output with the ping