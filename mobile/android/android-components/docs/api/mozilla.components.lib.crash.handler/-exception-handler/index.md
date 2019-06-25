[android-components](../../index.md) / [mozilla.components.lib.crash.handler](../index.md) / [ExceptionHandler](./index.md)

# ExceptionHandler

`class ExceptionHandler : `[`UncaughtExceptionHandler`](https://developer.android.com/reference/java/lang/Thread/UncaughtExceptionHandler.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/handler/ExceptionHandler.kt#L15)

[Thread.UncaughtExceptionHandler](https://developer.android.com/reference/java/lang/Thread/UncaughtExceptionHandler.html) implementation that forwards crashes to the [CrashReporter](../../mozilla.components.lib.crash/-crash-reporter/index.md) instance.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ExceptionHandler(context: <ERROR CLASS>, crashReporter: `[`CrashReporter`](../../mozilla.components.lib.crash/-crash-reporter/index.md)`, defaultExceptionHandler: `[`UncaughtExceptionHandler`](https://developer.android.com/reference/java/lang/Thread/UncaughtExceptionHandler.html)`? = null)`<br>[Thread.UncaughtExceptionHandler](https://developer.android.com/reference/java/lang/Thread/UncaughtExceptionHandler.html) implementation that forwards crashes to the [CrashReporter](../../mozilla.components.lib.crash/-crash-reporter/index.md) instance. |

### Functions

| Name | Summary |
|---|---|
| [uncaughtException](uncaught-exception.md) | `fun uncaughtException(thread: `[`Thread`](https://developer.android.com/reference/java/lang/Thread.html)`, throwable: `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
