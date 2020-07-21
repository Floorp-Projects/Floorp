[android-components](../../index.md) / [mozilla.components.support.android.test.rules](../index.md) / [WebserverRule](./index.md)

# WebserverRule

`class WebserverRule : TestWatcher` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/android-test/src/main/java/mozilla/components/support/android/test/rules/WebserverRule.kt#L21)

A [TestWatcher](#) junit rule that will serve content from assets in the test package.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WebserverRule()`<br>A [TestWatcher](#) junit rule that will serve content from assets in the test package. |

### Functions

| Name | Summary |
|---|---|
| [finished](finished.md) | `fun finished(description: Description?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [starting](starting.md) | `fun starting(description: Description?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [url](url.md) | `fun url(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = ""): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
