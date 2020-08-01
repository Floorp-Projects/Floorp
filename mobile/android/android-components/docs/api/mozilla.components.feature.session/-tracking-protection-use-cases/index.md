[android-components](../../index.md) / [mozilla.components.feature.session](../index.md) / [TrackingProtectionUseCases](./index.md)

# TrackingProtectionUseCases

`class TrackingProtectionUseCases` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/TrackingProtectionUseCases.kt#L21)

Contains use cases related to the tracking protection.

### Parameters

`store` - the application's [BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md).

`engine` - the application's [Engine](../../mozilla.components.concept.engine/-engine/index.md).

### Types

| Name | Summary |
|---|---|
| [AddExceptionUseCase](-add-exception-use-case/index.md) | `class AddExceptionUseCase`<br>Use case for adding a new tab to the exception list. |
| [ContainsExceptionUseCase](-contains-exception-use-case/index.md) | `class ContainsExceptionUseCase`<br>Use case for verifying if a tab is in the exception list. |
| [FetchExceptionsUseCase](-fetch-exceptions-use-case/index.md) | `class FetchExceptionsUseCase`<br>Use case for fetching all exceptions in the exception list. |
| [FetchTrackingLogUserCase](-fetch-tracking-log-user-case/index.md) | `class FetchTrackingLogUserCase`<br>Use case for fetching all the tracking protection logged information. |
| [RemoveAllExceptionsUseCase](-remove-all-exceptions-use-case/index.md) | `class RemoveAllExceptionsUseCase`<br>Use case for removing all tabs from the exception list. |
| [RemoveExceptionUseCase](-remove-exception-use-case/index.md) | `class RemoveExceptionUseCase`<br>Use case for removing a tab or a [TrackingProtectionException](../../mozilla.components.concept.engine.content.blocking/-tracking-protection-exception/index.md) from the exception list. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TrackingProtectionUseCases(store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`)`<br>Contains use cases related to the tracking protection. |

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
| [store](store.md) | `val store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)<br>the application's [BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
