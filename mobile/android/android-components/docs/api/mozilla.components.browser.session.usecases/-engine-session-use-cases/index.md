[android-components](../../index.md) / [mozilla.components.browser.session.usecases](../index.md) / [EngineSessionUseCases](./index.md)

# EngineSessionUseCases

`class EngineSessionUseCases` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/usecases/EngineSessionUseCases.kt#L16)

UseCases for getting and creating [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) instances for tabs.

This class and its use cases are an interim solution in order to migrate components away from
using SessionManager for getting and creating [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) instances.

### Types

| Name | Summary |
|---|---|
| [GetOrCreateUseCase](-get-or-create-use-case/index.md) | `class GetOrCreateUseCase`<br>Use case for getting or creating an [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) for a tab. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `EngineSessionUseCases(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`)`<br>UseCases for getting and creating [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) instances for tabs. |

### Properties

| Name | Summary |
|---|---|
| [getOrCreateEngineSession](get-or-create-engine-session.md) | `val getOrCreateEngineSession: `[`GetOrCreateUseCase`](-get-or-create-use-case/index.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
