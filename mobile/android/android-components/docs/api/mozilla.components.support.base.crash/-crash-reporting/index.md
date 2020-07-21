[android-components](../../index.md) / [mozilla.components.support.base.crash](../index.md) / [CrashReporting](./index.md)

# CrashReporting

`interface CrashReporting` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/crash/CrashReporting.kt#L12)

A  crash reporter interface that can report caught exception to multiple services.

### Functions

| Name | Summary |
|---|---|
| [recordCrashBreadcrumb](record-crash-breadcrumb.md) | `abstract fun recordCrashBreadcrumb(breadcrumb: `[`Breadcrumb`](../-breadcrumb/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Add a crash breadcrumb to all registered services with breadcrumb support. |
| [submitCaughtException](submit-caught-exception.md) | `abstract fun submitCaughtException(throwable: `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`): Job`<br>Submit a caught exception report to all registered services. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [CrashReporter](../../mozilla.components.lib.crash/-crash-reporter/index.md) | `class CrashReporter : `[`CrashReporting`](./index.md)<br>A generic crash reporter that can report crashes to multiple services. |
