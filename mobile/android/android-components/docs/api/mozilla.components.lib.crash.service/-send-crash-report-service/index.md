[android-components](../../index.md) / [mozilla.components.lib.crash.service](../index.md) / [SendCrashReportService](./index.md)

# SendCrashReportService

`class SendCrashReportService` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/service/SendCrashReportService.kt#L31)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SendCrashReportService()` |

### Functions

| Name | Summary |
|---|---|
| [onBind](on-bind.md) | `fun onBind(intent: <ERROR CLASS>): <ERROR CLASS>?` |
| [onStartCommand](on-start-command.md) | `fun onStartCommand(intent: <ERROR CLASS>, flags: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, startId: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Companion Object Functions

| Name | Summary |
|---|---|
| [createReportIntent](create-report-intent.md) | `fun createReportIntent(context: <ERROR CLASS>, crash: `[`Crash`](../../mozilla.components.lib.crash/-crash/index.md)`, notificationTag: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, notificationId: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0): <ERROR CLASS>` |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
