[android-components](../../index.md) / [mozilla.components.lib.crash.service](../index.md) / [SentryService](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`SentryService(context: <ERROR CLASS>, dsn: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, tags: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = emptyMap(), environment: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, sendEventForNativeCrashes: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, clientFactory: SentryClientFactory = AndroidSentryClientFactory(context))`

A [CrashReporterService](../-crash-reporter-service/index.md) implementation that uploads crash reports to a Sentry server.

This implementation will add default tags to every sent crash report (like the used Android Components version)
prefixed with "ac.".

### Parameters

`context` - The application [Context](#).

`dsn` - Data Source Name of the Sentry server.

`tags` - A list of additional tags that will be sent together with crash reports.

`environment` - An optional, environment name string or null to set none