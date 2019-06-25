[android-components](../../index.md) / [mozilla.components.lib.crash](../index.md) / [CrashReporter](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`CrashReporter(services: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`CrashReporterService`](../../mozilla.components.lib.crash.service/-crash-reporter-service/index.md)`>, shouldPrompt: `[`Prompt`](-prompt/index.md)` = Prompt.NEVER, enabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, promptConfiguration: `[`PromptConfiguration`](-prompt-configuration/index.md)` = PromptConfiguration(), nonFatalCrashIntent: <ERROR CLASS>? = null)`

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

