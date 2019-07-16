[android-components](../../index.md) / [mozilla.components.lib.crash.service](../index.md) / [GleanCrashReporterService](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`GleanCrashReporterService(context: <ERROR CLASS>, file: `[`File`](https://developer.android.com/reference/java/io/File.html)` = File(context.applicationInfo.dataDir, CRASH_FILE_NAME))`

A [CrashReporterService](../-crash-reporter-service/index.md) implementation for recording metrics with Glean.  The purpose of this
crash reporter is to collect crash count metrics by capturing [Crash.UncaughtExceptionCrash](../../mozilla.components.lib.crash/-crash/-uncaught-exception-crash/index.md) and
[Crash.NativeCodeCrash](../../mozilla.components.lib.crash/-crash/-native-code-crash/index.md) events and record to the respective
[mozilla.components.service.glean.private.CounterMetricType](../../mozilla.components.service.glean.private/-counter-metric-type/index.md).

