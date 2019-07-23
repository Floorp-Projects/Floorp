[android-components](../../index.md) / [mozilla.components.concept.engine.content.blocking](../index.md) / [Tracker](./index.md)

# Tracker

`class Tracker` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/content/blocking/Tracker.kt#L12)

Represents a blocked content tracker.

### Types

| Name | Summary |
|---|---|
| [Category](-category/index.md) | `enum class Category` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Tracker(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, categories: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Category`](-category/index.md)`> = emptyList())`<br>Represents a blocked content tracker. |

### Properties

| Name | Summary |
|---|---|
| [categories](categories.md) | `val categories: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Category`](-category/index.md)`>`<br>A list of categories that this [Tracker](./index.md) belongs. |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The URL of the tracker. |
