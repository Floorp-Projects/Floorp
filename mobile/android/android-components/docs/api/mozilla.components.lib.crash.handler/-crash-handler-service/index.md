[android-components](../../index.md) / [mozilla.components.lib.crash.handler](../index.md) / [CrashHandlerService](./index.md)

# CrashHandlerService

`class CrashHandlerService : JobIntentService` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/handler/CrashHandlerService.kt#L15)

Service receiving native code crashes (from GeckoView).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `CrashHandlerService()`<br>Service receiving native code crashes (from GeckoView). |

### Functions

| Name | Summary |
|---|---|
| [onHandleWork](on-handle-work.md) | `fun onHandleWork(intent: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onStartCommand](on-start-command.md) | `fun onStartCommand(intent: <ERROR CLASS>?, flags: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, startId: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
