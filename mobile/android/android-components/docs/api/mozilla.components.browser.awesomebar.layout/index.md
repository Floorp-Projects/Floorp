[android-components](../index.md) / [mozilla.components.browser.awesomebar.layout](./index.md)

## Package mozilla.components.browser.awesomebar.layout

### Types

| Name | Summary |
|---|---|
| [DefaultSuggestionLayout](-default-suggestion-layout/index.md) | `class DefaultSuggestionLayout : `[`SuggestionLayout`](-suggestion-layout/index.md)<br>Default implementation of [SuggestionLayout](-suggestion-layout/index.md) to be used by [BrowserAwesomeBar](../mozilla.components.browser.awesomebar/-browser-awesome-bar/index.md). |
| [SuggestionLayout](-suggestion-layout/index.md) | `interface SuggestionLayout`<br>A [SuggestionLayout](-suggestion-layout/index.md) implementation defines how the suggestions of the [BrowserAwesomeBar](../mozilla.components.browser.awesomebar/-browser-awesome-bar/index.md) are getting layout. By default [BrowserAwesomeBar](../mozilla.components.browser.awesomebar/-browser-awesome-bar/index.md) uses [DefaultSuggestionLayout](-default-suggestion-layout/index.md). However a consumer can provide its own implementation in order to create a customized look &amp; feel. |
| [SuggestionViewHolder](-suggestion-view-holder/index.md) | `abstract class SuggestionViewHolder`<br>A view holder implementation for displaying an [AwesomeBar.Suggestion](../mozilla.components.concept.awesomebar/-awesome-bar/-suggestion/index.md). |
