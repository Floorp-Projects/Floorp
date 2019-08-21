[android-components](../../index.md) / [mozilla.components.browser.toolbar](../index.md) / [AsyncAutocompleteDelegate](./index.md)

# AsyncAutocompleteDelegate

`class AsyncAutocompleteDelegate : `[`AutocompleteDelegate`](../../mozilla.components.concept.toolbar/-autocomplete-delegate/index.md)`, CoroutineScope` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/toolbar/src/main/java/mozilla/components/browser/toolbar/BrowserToolbar.kt#L839)

An autocomplete delegate which is aware of its parent scope (to check for cancellations).
Responsible for processing autocompletion results and discarding stale results when [urlView](#) moved on.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AsyncAutocompleteDelegate(urlView: `[`AutocompleteView`](../../mozilla.components.ui.autocomplete/-autocomplete-view/index.md)`, parentScope: CoroutineScope, coroutineContext: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)`, logger: `[`Logger`](../../mozilla.components.support.base.log.logger/-logger/index.md)` = Logger("AsyncAutocompleteDelegate"))`<br>An autocomplete delegate which is aware of its parent scope (to check for cancellations). Responsible for processing autocompletion results and discarding stale results when [urlView](#) moved on. |

### Properties

| Name | Summary |
|---|---|
| [coroutineContext](coroutine-context.md) | `val coroutineContext: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html) |

### Functions

| Name | Summary |
|---|---|
| [applyAutocompleteResult](apply-autocomplete-result.md) | `fun applyAutocompleteResult(result: `[`AutocompleteResult`](../../mozilla.components.concept.toolbar/-autocomplete-result/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [noAutocompleteResult](no-autocomplete-result.md) | `fun noAutocompleteResult(input: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Autocompletion was invoked and no match was returned. |

### Extension Functions

| Name | Summary |
|---|---|
| [launchGeckoResult](../../mozilla.components.browser.engine.gecko/kotlinx.coroutines.-coroutine-scope/launch-gecko-result.md) | `fun <T> CoroutineScope.launchGeckoResult(context: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = EmptyCoroutineContext, start: CoroutineStart = CoroutineStart.DEFAULT, block: suspend CoroutineScope.() -> `[`T`](../../mozilla.components.browser.engine.gecko/kotlinx.coroutines.-coroutine-scope/launch-gecko-result.md#T)`): `[`GeckoResult`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoResult.html)`<`[`T`](../../mozilla.components.browser.engine.gecko/kotlinx.coroutines.-coroutine-scope/launch-gecko-result.md#T)`>`<br>Create a GeckoResult from a co-routine. |
