[android-components](../../index.md) / [mozilla.components.feature.search](../index.md) / [SearchAdapter](./index.md)

# SearchAdapter

`interface SearchAdapter` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/search/src/main/java/mozilla/components/feature/search/SearchAdapter.kt#L10)

May be implemented by client code in order to allow a component to start searches.

### Functions

| Name | Summary |
|---|---|
| [isPrivateSession](is-private-session.md) | `abstract fun isPrivateSession(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Called by the component to check whether or not the currently selected session is private. |
| [sendSearch](send-search.md) | `abstract fun sendSearch(isPrivate: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called by the component to indicate that the user should be shown a search. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [BrowserStoreSearchAdapter](../-browser-store-search-adapter/index.md) | `class BrowserStoreSearchAdapter : `[`SearchAdapter`](./index.md)<br>Adapter which wraps a [browserStore](#) in order to fulfill the [SearchAdapter](./index.md) interface. |
