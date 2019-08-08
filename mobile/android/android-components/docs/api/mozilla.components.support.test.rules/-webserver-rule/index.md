[android-components](../../index.md) / [mozilla.components.support.test.rules](../index.md) / [WebserverRule](./index.md)

# WebserverRule

`class WebserverRule : TestWatcher` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/test/src/main/java/mozilla/components/support/test/rules/WebserverRule.kt#L21)

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
