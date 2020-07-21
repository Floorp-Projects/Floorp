[android-components](../../index.md) / [mozilla.components.lib.crash](../index.md) / [CrashReporter](./index.md)

# CrashReporter

`class CrashReporter : `[`CrashReporting`](../../mozilla.components.support.base.crash/-crash-reporting/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/CrashReporter.kt#L62)

A generic crash reporter that can report crashes to multiple services.

In the `onCreate()` method of your Application class create a `CrashReporter` instance and call `install()`:

``` Kotlin
CrashReporter(
  services = listOf(
    // List the crash reporting services you want to use
  )
).install(this)
```

With this minimal setup the crash reporting library will capture "uncaught exception" crashes and "native code"
crashes and forward them to the configured crash reporting services.

### Types

| Name | Summary |
|---|---|
| [Prompt](-prompt/index.md) | `enum class Prompt` |
| [PromptConfiguration](-prompt-configuration/index.md) | `data class PromptConfiguration`<br>Configuration for the crash reporter prompt. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `CrashReporter(context: <ERROR CLASS>, services: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`CrashReporterService`](../../mozilla.components.lib.crash.service/-crash-reporter-service/index.md)`> = emptyList(), telemetryServices: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`CrashTelemetryService`](../../mozilla.components.lib.crash.service/-crash-telemetry-service/index.md)`> = emptyList(), shouldPrompt: `[`Prompt`](-prompt/index.md)` = Prompt.NEVER, enabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, promptConfiguration: `[`PromptConfiguration`](-prompt-configuration/index.md)` = PromptConfiguration(), nonFatalCrashIntent: <ERROR CLASS>? = null, scope: CoroutineScope = CoroutineScope(Dispatchers.IO))`<br>A generic crash reporter that can report crashes to multiple services. |

### Properties

| Name | Summary |
|---|---|
| [enabled](enabled.md) | `var enabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Enable/Disable crash reporting. |

### Functions

| Name | Summary |
|---|---|
| [install](install.md) | `fun install(applicationContext: <ERROR CLASS>): `[`CrashReporter`](./index.md)<br>Install this [CrashReporter](./index.md) instance. At this point the component will be setup to collect crash reports. |
| [recordCrashBreadcrumb](record-crash-breadcrumb.md) | `fun recordCrashBreadcrumb(breadcrumb: `[`Breadcrumb`](../../mozilla.components.support.base.crash/-breadcrumb/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Add a crash breadcrumb to all registered services with breadcrumb support. |
| [submitCaughtException](submit-caught-exception.md) | `fun submitCaughtException(throwable: `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`): Job`<br>Submit a caught exception report to all registered services. |
| [submitCrashTelemetry](submit-crash-telemetry.md) | `fun submitCrashTelemetry(crash: `[`Crash`](../-crash/index.md)`, then: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {}): Job`<br>Submit a crash report to all registered telemetry services. |
| [submitReport](submit-report.md) | `fun submitReport(crash: `[`Crash`](../-crash/index.md)`, then: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {}): Job`<br>Submit a crash report to all registered services. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
