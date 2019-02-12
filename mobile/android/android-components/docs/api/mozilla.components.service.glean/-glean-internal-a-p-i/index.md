[android-components](../../index.md) / [mozilla.components.service.glean](../index.md) / [GleanInternalAPI](./index.md)

# GleanInternalAPI

`open class GleanInternalAPI` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/Glean.kt#L31)

### Functions

| Name | Summary |
|---|---|
| [getUploadEnabled](get-upload-enabled.md) | `fun getUploadEnabled(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Get whether or not glean is allowed to record and upload data. |
| [handleEvent](handle-event.md) | `fun handleEvent(pingEvent: `[`PingEvent`](../-glean/-ping-event/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handle an event and send the appropriate pings. |
| [initialize](initialize.md) | `fun initialize(applicationContext: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, configuration: `[`Configuration`](../../mozilla.components.service.glean.config/-configuration/index.md)` = Configuration()): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Initialize glean. |
| [setExperimentActive](set-experiment-active.md) | `fun setExperimentActive(experimentId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, branch: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, extra: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Indicate that an experiment is running. Glean will then add an experiment annotation to the environment which is sent with pings. This information is not persisted between runs. |
| [setExperimentInactive](set-experiment-inactive.md) | `fun setExperimentInactive(experimentId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Indicate that an experiment is no longer running. |
| [setUploadEnabled](set-upload-enabled.md) | `fun setUploadEnabled(enabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Enable or disable glean collection and upload. |
| [testClearAllData](test-clear-all-data.md) | `fun testClearAllData(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Test only function to clear all storages and metrics.  Note that this also includes 'user' lifetime metrics so be aware that things like clientId will be wiped as well. |

### Inheritors

| Name | Summary |
|---|---|
| [Glean](../-glean/index.md) | `object Glean : `[`GleanInternalAPI`](./index.md) |
