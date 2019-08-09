[android-components](../../index.md) / [mozilla.components.service.glean](../index.md) / [GleanInternalAPI](./index.md)

# GleanInternalAPI

`open class GleanInternalAPI` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/Glean.kt#L44)

### Functions

| Name | Summary |
|---|---|
| [getUploadEnabled](get-upload-enabled.md) | `fun getUploadEnabled(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Get whether or not Glean is allowed to record and upload data. |
| [handleBackgroundEvent](handle-background-event.md) | `fun handleBackgroundEvent(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handle the background event and send the appropriate pings. |
| [initialize](initialize.md) | `fun initialize(applicationContext: <ERROR CLASS>, configuration: `[`Configuration`](../../mozilla.components.service.glean.config/-configuration/index.md)` = Configuration()): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Initialize Glean. |
| [isInitialized](is-initialized.md) | `fun isInitialized(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if the Glean library has been initialized. |
| [registerPings](register-pings.md) | `fun registerPings(pings: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Register the pings generated from `pings.yaml` with Glean. |
| [setExperimentActive](set-experiment-active.md) | `fun setExperimentActive(experimentId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, branch: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, extra: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Indicate that an experiment is running. Glean will then add an experiment annotation to the environment which is sent with pings. This information is not persisted between runs. |
| [setExperimentInactive](set-experiment-inactive.md) | `fun setExperimentInactive(experimentId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Indicate that an experiment is no longer running. |
| [setUploadEnabled](set-upload-enabled.md) | `fun setUploadEnabled(enabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Enable or disable Glean collection and upload. |
| [testGetExperimentData](test-get-experiment-data.md) | `fun testGetExperimentData(experimentId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`RecordedExperimentData`](../../mozilla.components.service.glean.storages/-recorded-experiment-data/index.md)<br>Returns the stored data for the requested active experiment, for testing purposes only. |
| [testIsExperimentActive](test-is-experiment-active.md) | `fun testIsExperimentActive(experimentId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Tests whether an experiment is active, for testing purposes only. |

### Inheritors

| Name | Summary |
|---|---|
| [Glean](../-glean.md) | `object Glean : `[`GleanInternalAPI`](./index.md) |
