[android-components](../../index.md) / [mozilla.components.browser.awesomebar.layout](../index.md) / [DefaultSuggestionLayout](index.md) / [getLayoutResource](./get-layout-resource.md)

# getLayoutResource

`fun getLayoutResource(suggestion: `[`Suggestion`](../../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md)`): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/awesomebar/src/main/java/mozilla/components/browser/awesomebar/layout/DefaultSuggestionLayout.kt#L16)

Overrides [SuggestionLayout.getLayoutResource](../-suggestion-layout/get-layout-resource.md)

Returns a layout resource ID to be used for this suggestion. The [BrowserAwesomeBar](../../mozilla.components.browser.awesomebar/-browser-awesome-bar/index.md) implementation will take
care of inflating the layout or re-using instances as needed.

