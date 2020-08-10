[android-components](../index.md) / [mozilla.components.support.utils](./index.md)

## Package mozilla.components.support.utils

### Types

| Name | Summary |
|---|---|
| [Browsers](-browsers/index.md) | `class Browsers`<br>Helpful tools for dealing with other browsers on this device. |
| [ColorUtils](-color-utils/index.md) | `object ColorUtils` |
| [DomainMatch](-domain-match/index.md) | `data class DomainMatch` |
| [DownloadUtils](-download-utils/index.md) | `object DownloadUtils` |
| [DrawableUtils](-drawable-utils/index.md) | `object DrawableUtils` |
| [RunWhenReadyQueue](-run-when-ready-queue/index.md) | `class RunWhenReadyQueue`<br>A queue that acts as a gate, either executing tasks right away if the queue is marked as "ready", i.e. gate is open, or queues them to be executed whenever the queue is marked as ready in the future, i.e. gate becomes open. |
| [SafeBundle](-safe-bundle/index.md) | `class SafeBundle`<br>See SafeIntent for more background: applications can put garbage values into Bundles. This is primarily experienced when there's garbage in the Intent's Bundle. However that Bundle can contain further bundles, and we need to handle those defensively too. |
| [SafeIntent](-safe-intent/index.md) | `class SafeIntent`<br>External applications can pass values into Intents that can cause us to crash: in defense, we wrap [Intent](#) and catch the exceptions they may force us to throw. See bug 1090385 for more. |
| [StatusBarUtils](-status-bar-utils/index.md) | `object StatusBarUtils` |
| [StorageUtils](-storage-utils/index.md) | `object StorageUtils` |
| [ThreadUtils](-thread-utils/index.md) | `object ThreadUtils` |
| [URLStringUtils](-u-r-l-string-utils/index.md) | `object URLStringUtils` |
| [WebURLFinder](-web-u-r-l-finder/index.md) | `class WebURLFinder`<br>Regular expressions used in this class are taken from Android's Patterns.java. We brought them in to standardize URL matching across Android versions, instead of relying on Android version-dependent built-ins that can vary across Android versions. The original code can be found here: http://androidxref.com/8.0.0_r4/xref/frameworks/base/core/java/android/util/Patterns.java |

### Functions

| Name | Summary |
|---|---|
| [asForegroundServicePendingIntent](as-foreground-service-pending-intent.md) | `fun <ERROR CLASS>.asForegroundServicePendingIntent(context: <ERROR CLASS>, requestCode: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, flags: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = PendingIntent.FLAG_UPDATE_CURRENT): <ERROR CLASS>`<br>Create a [PendingIntent](#) instance to run a certain service described with the [Intent](#). |
| [logElapsedTime](log-elapsed-time.md) | `fun <T> logElapsedTime(logger: `[`Logger`](../mozilla.components.support.base.log.logger/-logger/index.md)`, op: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, block: () -> `[`T`](log-elapsed-time.md#T)`): `[`T`](log-elapsed-time.md#T)<br>Executes the given [block](log-elapsed-time.md#mozilla.components.support.utils$logElapsedTime(mozilla.components.support.base.log.logger.Logger, kotlin.String, kotlin.Function0((mozilla.components.support.utils.logElapsedTime.T)))/block) and logs the elapsed time in milliseconds. Uses [System.nanoTime](http://docs.oracle.com/javase/7/docs/api/java/lang/System.html#nanoTime()) for measurements, since it isn't tied to a wall-clock. |
| [segmentAwareDomainMatch](segment-aware-domain-match.md) | `fun segmentAwareDomainMatch(query: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, urls: `[`Iterable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-iterable/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`DomainMatch`](-domain-match/index.md)`?` |
| [toSafeBundle](to-safe-bundle.md) | `fun <ERROR CLASS>.toSafeBundle(): <ERROR CLASS>`<br>Returns a [SafeBundle](-safe-bundle/index.md) for the given [Bundle](#). |
| [toSafeIntent](to-safe-intent.md) | `fun <ERROR CLASS>.toSafeIntent(): `[`SafeIntent`](-safe-intent/index.md)<br>Returns a [SafeIntent](-safe-intent/index.md) for the given [Intent](#). |
