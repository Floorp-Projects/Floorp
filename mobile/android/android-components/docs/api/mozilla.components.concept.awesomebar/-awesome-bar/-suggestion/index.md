[android-components](../../../index.md) / [mozilla.components.concept.awesomebar](../../index.md) / [AwesomeBar](../index.md) / [Suggestion](./index.md)

# Suggestion

`data class Suggestion` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/awesomebar/src/main/java/mozilla/components/concept/awesomebar/AwesomeBar.kt#L82)

A [Suggestion](./index.md) to be displayed by an [AwesomeBar](../index.md) implementation.

### Types

| Name | Summary |
|---|---|
| [Chip](-chip/index.md) | `data class Chip`<br>Chips are compact actions that are shown as part of a suggestion. For example a [Suggestion](./index.md) from a search engine may offer multiple search suggestion chips for different search terms. |
| [Flag](-flag/index.md) | `enum class Flag`<br>Flags can be added by a [SuggestionProvider](../-suggestion-provider/index.md) to help the [AwesomeBar](../index.md) implementation decide how to display a specific [Suggestion](./index.md). For example an [AwesomeBar](../index.md) could display a bookmark star icon next to [Suggestion](./index.md)s that contain the [BOOKMARK](-flag/-b-o-o-k-m-a-r-k.md) flag. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Suggestion(provider: `[`SuggestionProvider`](../-suggestion-provider/index.md)`, id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = UUID.randomUUID().toString(), title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, description: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, icon: suspend (width: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, height: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`) -> <ERROR CLASS>? = null, chips: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Chip`](-chip/index.md)`> = emptyList(), flags: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`Flag`](-flag/index.md)`> = emptySet(), onSuggestionClicked: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null, onChipClicked: (`[`Chip`](-chip/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null, score: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0)`<br>A [Suggestion](./index.md) to be displayed by an [AwesomeBar](../index.md) implementation. |

### Properties

| Name | Summary |
|---|---|
| [chips](chips.md) | `val chips: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Chip`](-chip/index.md)`>`<br>A list of [Chip](-chip/index.md) instances to be displayed. |
| [description](description.md) | `val description: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>A user-readable description for the [Suggestion](./index.md). |
| [flags](flags.md) | `val flags: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`Flag`](-flag/index.md)`>`<br>A set of [Flag](-flag/index.md) values for this [Suggestion](./index.md). |
| [icon](icon.md) | `val icon: suspend (width: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, height: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`) -> <ERROR CLASS>?`<br>A lambda that can be invoked by the [AwesomeBar](../index.md) implementation to receive an icon [Bitmap](#) for this [Suggestion](./index.md). The [AwesomeBar](../index.md) will pass in its desired width and height for the Bitmap. |
| [id](id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>A unique ID (provider scope) identifying this [Suggestion](./index.md). A stable ID but different data indicates to the [AwesomeBar](../index.md) that this is the same [Suggestion](./index.md) with new data. This will affect how the [AwesomeBar](../index.md) animates showing the new suggestion. |
| [onChipClicked](on-chip-clicked.md) | `val onChipClicked: (`[`Chip`](-chip/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A callback to be executed when a [Chip](-chip/index.md) was clicked by the user. |
| [onSuggestionClicked](on-suggestion-clicked.md) | `val onSuggestionClicked: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A callback to be executed when the [Suggestion](./index.md) was clicked by the user. |
| [provider](provider.md) | `val provider: `[`SuggestionProvider`](../-suggestion-provider/index.md)<br>The provider this suggestion came from. |
| [score](score.md) | `val score: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>A score used to rank suggestions of this provider against each other. A suggestion with a higher score will be shown on top of suggestions with a lower score. |
| [title](title.md) | `val title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>A user-readable title for the [Suggestion](./index.md). |

### Functions

| Name | Summary |
|---|---|
| [areContentsTheSame](are-contents-the-same.md) | `fun areContentsTheSame(other: `[`Suggestion`](./index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if the content of the two suggestions is the same. |
