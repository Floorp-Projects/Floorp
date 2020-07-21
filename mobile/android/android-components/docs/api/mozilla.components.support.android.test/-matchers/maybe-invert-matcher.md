[android-components](../../index.md) / [mozilla.components.support.android.test](../index.md) / [Matchers](index.md) / [maybeInvertMatcher](./maybe-invert-matcher.md)

# maybeInvertMatcher

`fun <T> maybeInvertMatcher(matcher: Matcher<`[`T`](maybe-invert-matcher.md#T)`>, useUnmodifiedMatcher: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): Matcher<`[`T`](maybe-invert-matcher.md#T)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/android-test/src/main/java/mozilla/components/support/android/test/Matchers.kt#L22)

Conditionally applies the [not](#) matcher based on the given argument: inverts the matcher when
[useUnmodifiedMatcher](maybe-invert-matcher.md#mozilla.components.support.android.test.Matchers$maybeInvertMatcher(org.hamcrest.Matcher((mozilla.components.support.android.test.Matchers.maybeInvertMatcher.T)), kotlin.Boolean)/useUnmodifiedMatcher) is false, otherwise returns the matcher unmodified.

This allows developers to write code more generically by using a boolean argument: e.g. assertIsShown(Boolean)
rather than two methods, assertIsShown() and assertIsNotShown().

