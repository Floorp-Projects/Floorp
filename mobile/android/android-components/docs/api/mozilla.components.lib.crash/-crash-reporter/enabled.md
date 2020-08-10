[android-components](../../index.md) / [mozilla.components.lib.crash](../index.md) / [CrashReporter](index.md) / [enabled](./enabled.md)

# enabled

`var enabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/CrashReporter.kt#L67)

Enable/Disable crash reporting.

### Property

`enabled` - Enable/Disable crash reporting.

### Parameters

`services` - List of crash reporting services that should receive crash reports.

`telemetryServices` - List of telemetry crash reporting services that should receive crash reports.

`shouldPrompt` - Whether or not the user should be prompted to confirm sending crash reports.

`enabled` - Enable/Disable crash reporting.

`promptConfiguration` - Configuration for customizing the crash reporter prompt.

`nonFatalCrashIntent` - A [PendingIntent](#) that will be launched if a non fatal crash (main process not affected)
    happened. This gives the app the opportunity to show an in-app confirmation UI before
    sending a crash report. See component README for details.