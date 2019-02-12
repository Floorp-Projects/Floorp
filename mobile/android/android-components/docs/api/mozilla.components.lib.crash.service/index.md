[android-components](../index.md) / [mozilla.components.lib.crash.service](./index.md)

## Package mozilla.components.lib.crash.service

### Types

| Name | Summary |
|---|---|
| [CrashReporterService](-crash-reporter-service/index.md) | `interface CrashReporterService`<br>Interface to be implemented by external services that accept crash reports. |
| [MozillaSocorroService](-mozilla-socorro-service/index.md) | `class MozillaSocorroService : `[`CrashReporterService`](-crash-reporter-service/index.md)<br>A [CrashReporterService](-crash-reporter-service/index.md) implementation uploading crash reports to crash-stats.mozilla.com. |
| [SentryService](-sentry-service/index.md) | `class SentryService : `[`CrashReporterService`](-crash-reporter-service/index.md)<br>A [CrashReporterService](-crash-reporter-service/index.md) implementation that uploads crash reports to a Sentry server. |

### Type Aliases

| Name | Summary |
|---|---|
| [GeckoCrashReporter](-gecko-crash-reporter.md) | `typealias GeckoCrashReporter = `[`CrashReporter`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/CrashReporter.html) |
