[android-components](../../index.md) / [mozilla.components.support.android.test](../index.md) / [Matchers](./index.md)

# Matchers

`object Matchers` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/android-test/src/main/java/mozilla/components/support/android/test/Matchers.kt#L13)

A collection of non-domain specific [Matcher](#)s.

### Functions

| Name | Summary |
|---|---|
| [maybeInvertMatcher](maybe-invert-matcher.md) | `fun <T> maybeInvertMatcher(matcher: Matcher<`[`T`](maybe-invert-matcher.md#T)`>, useUnmodifiedMatcher: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): Matcher<`[`T`](maybe-invert-matcher.md#T)`>`<br>Conditionally applies the [not](#) matcher based on the given argument: inverts the matcher when [useUnmodifiedMatcher](maybe-invert-matcher.md#mozilla.components.support.android.test.Matchers$maybeInvertMatcher(org.hamcrest.Matcher((mozilla.components.support.android.test.Matchers.maybeInvertMatcher.T)), kotlin.Boolean)/useUnmodifiedMatcher) is false, otherwise returns the matcher unmodified. |
