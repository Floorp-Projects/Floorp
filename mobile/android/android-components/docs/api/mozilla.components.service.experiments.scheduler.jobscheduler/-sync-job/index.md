[android-components](../../index.md) / [mozilla.components.service.experiments.scheduler.jobscheduler](../index.md) / [SyncJob](./index.md)

# SyncJob

`abstract class SyncJob : `[`JobService`](https://developer.android.com/reference/android/app/job/JobService.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/experiments/src/main/java/mozilla/components/service/experiments/scheduler/jobscheduler/SyncJob.kt#L15)

JobScheduler job used to updating the list of experiments

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SyncJob()`<br>JobScheduler job used to updating the list of experiments |

### Functions

| Name | Summary |
|---|---|
| [getExperiments](get-experiments.md) | `abstract fun getExperiments(): `[`Experiments`](../../mozilla.components.service.experiments/-experiments/index.md)<br>Used to provide the instance of Experiments the app is using |
| [onStartJob](on-start-job.md) | `open fun onStartJob(params: `[`JobParameters`](https://developer.android.com/reference/android/app/job/JobParameters.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onStopJob](on-stop-job.md) | `open fun onStopJob(params: `[`JobParameters`](https://developer.android.com/reference/android/app/job/JobParameters.html)`?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Extension Properties

| Name | Summary |
|---|---|
| [appVersionName](../../mozilla.components.support.ktx.android.content/android.content.-context/app-version-name.md) | `val `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.appVersionName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>The (visible) version name of the application, as specified by the  tag's versionName attribute. E.g. "2.0". |

### Extension Functions

| Name | Summary |
|---|---|
| [isOSOnLowMemory](../../mozilla.components.support.ktx.android.content/android.content.-context/is-o-s-on-low-memory.md) | `fun `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.isOSOnLowMemory(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns whether or not the operating system is under low memory conditions. |
| [isPermissionGranted](../../mozilla.components.support.ktx.android.content/android.content.-context/is-permission-granted.md) | `fun `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.isPermissionGranted(vararg permission: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns if a list of permission have been granted, if all the permission have been granted returns true otherwise false. |
| [share](../../mozilla.components.support.ktx.android.content/android.content.-context/share.md) | `fun `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.share(text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, subject: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = getString(R.string.mozac_support_ktx_share_dialog_title)): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Shares content via [ACTION_SEND](https://developer.android.com/reference/android/content/Intent.html#ACTION_SEND) intent. |
| [systemService](../../mozilla.components.support.ktx.android.content/android.content.-context/system-service.md) | `fun <T> `[`Context`](https://developer.android.com/reference/android/content/Context.html)`.systemService(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`T`](../../mozilla.components.support.ktx.android.content/android.content.-context/system-service.md#T)<br>Returns the handle to a system-level service by name. |
