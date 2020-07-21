[android-components](../../index.md) / [mozilla.components.browser.awesomebar](../index.md) / [BrowserAwesomeBar](./index.md)

# BrowserAwesomeBar

`class BrowserAwesomeBar : RecyclerView, `[`AwesomeBar`](../../mozilla.components.concept.awesomebar/-awesome-bar/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/awesomebar/src/main/java/mozilla/components/browser/awesomebar/BrowserAwesomeBar.kt#L36)

A customizable [AwesomeBar](../../mozilla.components.concept.awesomebar/-awesome-bar/index.md) implementation.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserAwesomeBar(context: <ERROR CLASS>, attrs: <ERROR CLASS>? = null, defStyleAttr: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0)`<br>A customizable [AwesomeBar](../../mozilla.components.concept.awesomebar/-awesome-bar/index.md) implementation. |

### Properties

| Name | Summary |
|---|---|
| [layout](layout.md) | `var layout: `[`SuggestionLayout`](../../mozilla.components.browser.awesomebar.layout/-suggestion-layout/index.md)<br>The [SuggestionLayout](../../mozilla.components.browser.awesomebar.layout/-suggestion-layout/index.md) implementation controls layout inflation and view binding for suggestions. |
| [transformer](transformer.md) | `var transformer: `[`SuggestionTransformer`](../../mozilla.components.browser.awesomebar.transform/-suggestion-transformer/index.md)`?`<br>An optional [SuggestionTransformer](../../mozilla.components.browser.awesomebar.transform/-suggestion-transformer/index.md) that receives [AwesomeBar.Suggestion](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md) objects from a [AwesomeBar.SuggestionProvider](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/index.md) and returns a new list of transformed [AwesomeBar.Suggestion](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md) objects. |

### Functions

| Name | Summary |
|---|---|
| [addProviders](add-providers.md) | `fun addProviders(vararg providers: `[`SuggestionProvider`](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds the following [SuggestionProvider](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/index.md) instances to be queried for [Suggestion](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md)s whenever the text changes. |
| [containsProvider](contains-provider.md) | `fun containsProvider(provider: `[`SuggestionProvider`](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns whether or not this awesome bar contains the following [SuggestionProvider](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/index.md) |
| [getUniqueSuggestionId](get-unique-suggestion-id.md) | `fun getUniqueSuggestionId(suggestion: `[`Suggestion`](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md)`): `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>Returns a unique suggestion ID to make sure ID's can't collide across providers. |
| [onDetachedFromWindow](on-detached-from-window.md) | `fun onDetachedFromWindow(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onInputCancelled](on-input-cancelled.md) | `fun onInputCancelled(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Fired when the user has cancelled their interaction with the awesome bar. |
| [onInputChanged](on-input-changed.md) | `fun onInputChanged(text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Fired whenever the user changes their input, after they have started interacting with the awesome bar. |
| [onInputStarted](on-input-started.md) | `fun onInputStarted(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Fired when the user starts interacting with the awesome bar by entering text in the toolbar. |
| [removeAllProviders](remove-all-providers.md) | `fun removeAllProviders(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes all [SuggestionProvider](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/index.md)s |
| [removeProviders](remove-providers.md) | `fun removeProviders(vararg providers: `[`SuggestionProvider`](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes the following [SuggestionProvider](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/index.md) |
| [setOnStopListener](set-on-stop-listener.md) | `fun setOnStopListener(listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds a lambda to be invoked when the user has finished interacting with the awesome bar (e.g. selected a suggestion). |

### Inherited Functions

| Name | Summary |
|---|---|
| [asView](../../mozilla.components.concept.awesomebar/-awesome-bar/as-view.md) | `open fun asView(): <ERROR CLASS>`<br>Casts this awesome bar to an Android View object. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
