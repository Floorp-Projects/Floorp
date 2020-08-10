[android-components](../../../index.md) / [mozilla.components.concept.awesomebar](../../index.md) / [AwesomeBar](../index.md) / [Suggestion](index.md) / [areContentsTheSame](./are-contents-the-same.md)

# areContentsTheSame

`fun areContentsTheSame(other: `[`Suggestion`](index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/awesomebar/src/main/java/mozilla/components/concept/awesomebar/AwesomeBar.kt#L132)

Returns true if the content of the two suggestions is the same.

This is used by [AwesomeBar](../index.md) implementations to decide whether an updated suggestion (same id) needs its
view to be updated in order to display new data.

