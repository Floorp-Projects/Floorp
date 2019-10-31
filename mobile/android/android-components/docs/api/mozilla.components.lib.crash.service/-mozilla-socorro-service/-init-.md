[android-components](../../index.md) / [mozilla.components.lib.crash.service](../index.md) / [MozillaSocorroService](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`MozillaSocorroService(applicationContext: <ERROR CLASS>, appName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, appId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = MOZILLA_PRODUCT_ID, version: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = BuildConfig.MOZILLA_VERSION, buildId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = BuildConfig.MOZ_APP_BUILDID, vendor: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = BuildConfig.MOZ_APP_VENDOR, serverUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "https://crash-reports.mozilla.com/submit?id=$appId&version=$version&$buildId")`

A [CrashReporterService](../-crash-reporter-service/index.md) implementation uploading crash reports to crash-stats.mozilla.com.

### Parameters

`applicationContext` - The application [Context](#).

`appName` - A human-readable app name. This name is used on crash-stats.mozilla.com to filter crashes by app.
    The name needs to be whitelisted for the server to accept the crash.
    [File a bug](https://bugzilla.mozilla.org/enter_bug.cgi?product=Socorro) if you would like to get your
    app added to the whitelist.