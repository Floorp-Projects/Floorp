[android-components](../../index.md) / [mozilla.components.feature.session](../index.md) / [TrackingProtectionUseCases](./index.md)

# TrackingProtectionUseCases

`class TrackingProtectionUseCases` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/TrackingProtectionUseCases.kt#L19)

Contains use cases related to the tracking protection.

### Parameters

`sessionManager` - the application's [SessionManager](../../mozilla.components.browser.session/-session-manager/index.md).

`engine` - the application's [Engine](../../mozilla.components.concept.engine/-engine/index.md).

### Types

| Name | Summary |
|---|---|
| [FetchTrackingLogUserCase](-fetch-tracking-log-user-case/index.md) | `class FetchTrackingLogUserCase` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TrackingProtectionUseCases(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`)`<br>Contains use cases related to the tracking protection. |

### Properties

| Name | Summary |
|---|---|
| [engine](engine.md) | `val engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)<br>the application's [Engine](../../mozilla.components.concept.engine/-engine/index.md). |
| [fetchTrackingLogs](fetch-tracking-logs.md) | `val fetchTrackingLogs: `[`FetchTrackingLogUserCase`](-fetch-tracking-log-user-case/index.md) |
| [sessionManager](session-manager.md) | `val sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)<br>the application's [SessionManager](../../mozilla.components.browser.session/-session-manager/index.md). |
