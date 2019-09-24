[android-components](../../index.md) / [mozilla.components.browser.awesomebar.layout](../index.md) / [SuggestionViewHolder](index.md) / [recycle](./recycle.md)

# recycle

`open fun recycle(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/awesomebar/src/main/java/mozilla/components/browser/awesomebar/layout/SuggestionViewHolder.kt#L29)

Notifies this [SuggestionViewHolder](index.md) that it has been recycled. If this holder (or its views) keep references to
large or expensive data such as large bitmaps, this may be a good place to release those resources.

