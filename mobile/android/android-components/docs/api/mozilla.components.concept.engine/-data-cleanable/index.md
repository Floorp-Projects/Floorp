[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [DataCleanable](./index.md)

# DataCleanable

`interface DataCleanable` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/DataCleanable.kt#L11)

Contract to indicate how objects with the ability to clear data should behave.

### Functions

| Name | Summary |
|---|---|
| [clearData](clear-data.md) | `open fun clearData(data: `[`BrowsingData`](../-engine/-browsing-data/index.md)` = Engine.BrowsingData.all(), host: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, onSuccess: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }, onError: (`[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Clears browsing data stored. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [Engine](../-engine/index.md) | `interface Engine : `[`WebExtensionRuntime`](../../mozilla.components.concept.engine.webextension/-web-extension-runtime/index.md)`, `[`DataCleanable`](./index.md)<br>Entry point for interacting with the engine implementation. |
| [EngineSession](../-engine-session/index.md) | `abstract class EngineSession : `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](../-engine-session/-observer/index.md)`>, `[`DataCleanable`](./index.md)<br>Class representing a single engine session. |
