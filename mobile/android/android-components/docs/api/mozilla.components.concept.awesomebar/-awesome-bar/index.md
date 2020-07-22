[android-components](../../index.md) / [mozilla.components.concept.awesomebar](../index.md) / [AwesomeBar](./index.md)

# AwesomeBar

`interface AwesomeBar` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/awesomebar/src/main/java/mozilla/components/concept/awesomebar/AwesomeBar.kt#L20)

Interface to be implemented by awesome bar implementations.

An awesome bar has multiple duties:

* Display [Suggestion](-suggestion/index.md) instances and invoking its callbacks once selected
* React to outside events: [onInputStarted](on-input-started.md), [onInputChanged](on-input-changed.md), [onInputCancelled](on-input-cancelled.md).
* Query [SuggestionProvider](-suggestion-provider/index.md) instances for new suggestions when the text changes.

### Types

| Name | Summary |
|---|---|
| [Suggestion](-suggestion/index.md) | `data class Suggestion`<br>A [Suggestion](-suggestion/index.md) to be displayed by an [AwesomeBar](./index.md) implementation. |
| [SuggestionProvider](-suggestion-provider/index.md) | `interface SuggestionProvider`<br>A [SuggestionProvider](-suggestion-provider/index.md) is queried by an [AwesomeBar](./index.md) whenever the text in the address bar is changed by the user. It returns a list of [Suggestion](-suggestion/index.md)s to be displayed by the [AwesomeBar](./index.md). |

### Functions

| Name | Summary |
|---|---|
| [addProviders](add-providers.md) | `abstract fun addProviders(vararg providers: `[`SuggestionProvider`](-suggestion-provider/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds the following [SuggestionProvider](-suggestion-provider/index.md) instances to be queried for [Suggestion](-suggestion/index.md)s whenever the text changes. |
| [asView](as-view.md) | `open fun asView(): <ERROR CLASS>`<br>Casts this awesome bar to an Android View object. |
| [containsProvider](contains-provider.md) | `abstract fun containsProvider(provider: `[`SuggestionProvider`](-suggestion-provider/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns whether or not this awesome bar contains the following [SuggestionProvider](-suggestion-provider/index.md) |
| [onInputCancelled](on-input-cancelled.md) | `open fun onInputCancelled(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Fired when the user has cancelled their interaction with the awesome bar. |
| [onInputChanged](on-input-changed.md) | `abstract fun onInputChanged(text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Fired whenever the user changes their input, after they have started interacting with the awesome bar. |
| [onInputStarted](on-input-started.md) | `open fun onInputStarted(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Fired when the user starts interacting with the awesome bar by entering text in the toolbar. |
| [removeAllProviders](remove-all-providers.md) | `abstract fun removeAllProviders(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes all [SuggestionProvider](-suggestion-provider/index.md)s |
| [removeProviders](remove-providers.md) | `abstract fun removeProviders(vararg providers: `[`SuggestionProvider`](-suggestion-provider/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes the following [SuggestionProvider](-suggestion-provider/index.md) |
| [setOnStopListener](set-on-stop-listener.md) | `abstract fun setOnStopListener(listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds a lambda to be invoked when the user has finished interacting with the awesome bar (e.g. selected a suggestion). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [BrowserAwesomeBar](../../mozilla.components.browser.awesomebar/-browser-awesome-bar/index.md) | `class BrowserAwesomeBar : RecyclerView, `[`AwesomeBar`](./index.md)<br>A customizable [AwesomeBar](./index.md) implementation. |
