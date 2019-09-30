[android-components](../../index.md) / [mozilla.components.feature.session](../index.md) / [TrackingProtectionUseCases](./index.md)

# TrackingProtectionUseCases

`class TrackingProtectionUseCases` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/TrackingProtectionUseCases.kt#L20)

Contains use cases related to the tracking protection.

### Parameters

`sessionManager` - the application's [SessionManager](../../mozilla.components.browser.session/-session-manager/index.md).

`engine` - the application's [Engine](../../mozilla.components.concept.engine/-engine/index.md).

### Types

| Name | Summary |
|---|---|
| [AddExceptionUseCase](-add-exception-use-case/index.md) | `class AddExceptionUseCase`<br>Use case for adding a new [Session](../../mozilla.components.browser.session/-session/index.md) to the exception list. |
| [ContainsExceptionUseCase](-contains-exception-use-case/index.md) | `class ContainsExceptionUseCase`<br>Use case for verifying if a [Session](../../mozilla.components.browser.session/-session/index.md) is in the exception list. |
| [FetchExceptionsUseCase](-fetch-exceptions-use-case/index.md) | `class FetchExceptionsUseCase`<br>Use case for fetching all exceptions in the exception list. |
| [FetchTrackingLogUserCase](-fetch-tracking-log-user-case/index.md) | `class FetchTrackingLogUserCase`<br>Use case for fetching all the tracking protection logged information. |
| [RemoveAllExceptionsUseCase](-remove-all-exceptions-use-case/index.md) | `class RemoveAllExceptionsUseCase`<br>Use case for removing all [Session](../../mozilla.components.browser.session/-session/index.md)s from the exception list. |
| [RemoveExceptionUseCase](-remove-exception-use-case/index.md) | `class RemoveExceptionUseCase`<br>Use case for removing a [Session](../../mozilla.components.browser.session/-session/index.md) from the exception list. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TrackingProtectionUseCases(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`)`<br>Contains use cases related to the tracking protection. |

### Properties

| Name | Summary |
|---|---|
| [addException](add-exception.md) | `val addException: `[`AddExceptionUseCase`](-add-exception-use-case/index.md) |
| [containsException](contains-exception.md) | `val containsException: `[`ContainsExceptionUseCase`](-contains-exception-use-case/index.md) |
| [engine](engine.md) | `val engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)<br>the application's [Engine](../../mozilla.components.concept.engine/-engine/index.md). |
| [fetchExceptions](fetch-exceptions.md) | `val fetchExceptions: `[`FetchExceptionsUseCase`](-fetch-exceptions-use-case/index.md) |
| [fetchTrackingLogs](fetch-tracking-logs.md) | `val fetchTrackingLogs: `[`FetchTrackingLogUserCase`](-fetch-tracking-log-user-case/index.md) |
| [removeAllExceptions](remove-all-exceptions.md) | `val removeAllExceptions: `[`RemoveAllExceptionsUseCase`](-remove-all-exceptions-use-case/index.md) |
| [removeException](remove-exception.md) | `val removeException: `[`RemoveExceptionUseCase`](-remove-exception-use-case/index.md) |
| [sessionManager](session-manager.md) | `val sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)<br>the application's [SessionManager](../../mozilla.components.browser.session/-session-manager/index.md). |
