[android-components](../../index.md) / [mozilla.components.browser.awesomebar](../index.md) / [BrowserAwesomeBar](./index.md)

# BrowserAwesomeBar

`class BrowserAwesomeBar : RecyclerView, `[`AwesomeBar`](../../mozilla.components.concept.awesomebar/-awesome-bar/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/awesomebar/src/main/java/mozilla/components/browser/awesomebar/BrowserAwesomeBar.kt#L34)

A customizable [AwesomeBar](../../mozilla.components.concept.awesomebar/-awesome-bar/index.md) implementation.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserAwesomeBar(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, attrs: `[`AttributeSet`](https://developer.android.com/reference/android/util/AttributeSet.html)`? = null, defStyleAttr: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0)`<br>A customizable [AwesomeBar](../../mozilla.components.concept.awesomebar/-awesome-bar/index.md) implementation. |

### Properties

| Name | Summary |
|---|---|
| [layout](layout.md) | `var layout: `[`SuggestionLayout`](../../mozilla.components.browser.awesomebar.layout/-suggestion-layout/index.md)<br>The [SuggestionLayout](../../mozilla.components.browser.awesomebar.layout/-suggestion-layout/index.md) implementation controls layout inflation and view binding for suggestions. |
| [transformer](transformer.md) | `var transformer: `[`SuggestionTransformer`](../../mozilla.components.browser.awesomebar.transform/-suggestion-transformer/index.md)`?`<br>An optional [SuggestionTransformer](../../mozilla.components.browser.awesomebar.transform/-suggestion-transformer/index.md) that receives [AwesomeBar.Suggestion](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md) objects from a [AwesomeBar.SuggestionProvider](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/index.md) and returns a new list of transformed [AwesomeBar.Suggestion](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md) objects. |

### Functions

| Name | Summary |
|---|---|
| [addProviders](add-providers.md) | `fun addProviders(vararg providers: `[`SuggestionProvider`](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds the following [SuggestionProvider](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/index.md) instances to be queried for [Suggestion](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md)s whenever the text changes. |
| [onDetachedFromWindow](on-detached-from-window.md) | `fun onDetachedFromWindow(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onInputCancelled](on-input-cancelled.md) | `fun onInputCancelled(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Fired when the user has cancelled their interaction with the awesome bar. |
| [onInputChanged](on-input-changed.md) | `fun onInputChanged(text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Fired whenever the user changes their input, after they have started interacting with the awesome bar. |
| [onInputStarted](on-input-started.md) | `fun onInputStarted(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Fired when the user starts interacting with the awesome bar by entering text in the toolbar. |
| [removeAllProviders](remove-all-providers.md) | `fun removeAllProviders(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes all [SuggestionProviders](#) |
| [removeProviders](remove-providers.md) | `fun removeProviders(vararg providers: `[`SuggestionProvider`](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes the following [SuggestionProvider](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion-provider/index.md) |
| [setOnStopListener](set-on-stop-listener.md) | `fun setOnStopListener(listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds a lambda to be invoked when the user has finished interacting with the awesome bar (e.g. selected a suggestion). |

### Inherited Functions

| Name | Summary |
|---|---|
| [asView](../../mozilla.components.concept.awesomebar/-awesome-bar/as-view.md) | `open fun asView(): `[`View`](https://developer.android.com/reference/android/view/View.html)<br>Casts this awesome bar to an Android View object. |

### Extension Properties

| Name | Summary |
|---|---|
| [isLTR](../../mozilla.components.support.ktx.android.view/android.view.-view/is-l-t-r.md) | `val `[`View`](https://developer.android.com/reference/android/view/View.html)`.isLTR: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Is the horizontal layout direction of this view from Left to Right? |
| [isRTL](../../mozilla.components.support.ktx.android.view/android.view.-view/is-r-t-l.md) | `val `[`View`](https://developer.android.com/reference/android/view/View.html)`.isRTL: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Is the horizontal layout direction of this view from Right to Left? |

### Extension Functions

| Name | Summary |
|---|---|
| [forEach](../../mozilla.components.support.ktx.android.view/android.view.-view-group/for-each.md) | `fun `[`ViewGroup`](https://developer.android.com/reference/android/view/ViewGroup.html)`.forEach(action: (`[`View`](https://developer.android.com/reference/android/view/View.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Performs the given action on each View in this ViewGroup. |
| [hideKeyboard](../../mozilla.components.support.ktx.android.view/android.view.-view/hide-keyboard.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.hideKeyboard(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Hides the soft input window. |
| [isGone](../../mozilla.components.support.ktx.android.view/android.view.-view/is-gone.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.isGone(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if this view's visibility is set to View.GONE. |
| [isInvisible](../../mozilla.components.support.ktx.android.view/android.view.-view/is-invisible.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.isInvisible(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if this view's visibility is set to View.INVISIBLE. |
| [isVisible](../../mozilla.components.support.ktx.android.view/android.view.-view/is-visible.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.isVisible(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if this view's visibility is set to View.VISIBLE. |
| [setPadding](../../mozilla.components.support.ktx.android.view/android.view.-view/set-padding.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.setPadding(padding: `[`Padding`](../../mozilla.components.support.base.android/-padding/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Set a padding using [Padding](../../mozilla.components.support.base.android/-padding/index.md) object. |
| [showKeyboard](../../mozilla.components.support.ktx.android.view/android.view.-view/show-keyboard.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.showKeyboard(flags: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = InputMethodManager.SHOW_IMPLICIT): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Tries to focus this view and show the soft input window for it. |
