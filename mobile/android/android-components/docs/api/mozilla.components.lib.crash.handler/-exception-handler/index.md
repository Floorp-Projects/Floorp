[android-components](../../index.md) / [mozilla.components.lib.crash.handler](../index.md) / [ExceptionHandler](./index.md)

# ExceptionHandler

`class ExceptionHandler : `[`UncaughtExceptionHandler`](http://docs.oracle.com/javase/7/docs/api/java/lang/Thread/UncaughtExceptionHandler.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/handler/ExceptionHandler.kt#L18)

[Thread.UncaughtExceptionHandler](http://docs.oracle.com/javase/7/docs/api/java/lang/Thread/UncaughtExceptionHandler.html) implementation that forwards crashes to the [CrashReporter](../../mozilla.components.lib.crash/-crash-reporter/index.md) instance.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ExceptionHandler(context: <ERROR CLASS>, crashReporter: `[`CrashReporter`](../../mozilla.components.lib.crash/-crash-reporter/index.md)`, defaultExceptionHandler: `[`UncaughtExceptionHandler`](http://docs.oracle.com/javase/7/docs/api/java/lang/Thread/UncaughtExceptionHandler.html)`? = null)`<br>[Thread.UncaughtExceptionHandler](http://docs.oracle.com/javase/7/docs/api/java/lang/Thread/UncaughtExceptionHandler.html) implementation that forwards crashes to the [CrashReporter](../../mozilla.components.lib.crash/-crash-reporter/index.md) instance. |

### Functions

| Name | Summary |
|---|---|
| [uncaughtException](uncaught-exception.md) | `fun uncaughtException(thread: `[`Thread`](http://docs.oracle.com/javase/7/docs/api/java/lang/Thread.html)`, throwable: `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
