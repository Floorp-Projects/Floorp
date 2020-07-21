[android-components](../../index.md) / [mozilla.components.support.utils](../index.md) / [WebURLFinder](./index.md)

# WebURLFinder

`class WebURLFinder` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/utils/src/main/java/mozilla/components/support/utils/WebURLFinder.kt#L22)

Regular expressions used in this class are taken from Android's Patterns.java.
We brought them in to standardize URL matching across Android versions, instead of relying
on Android version-dependent built-ins that can vary across Android versions.
The original code can be found here:
http://androidxref.com/8.0.0_r4/xref/frameworks/base/core/java/android/util/Patterns.java

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WebURLFinder(string: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?)` |

### Functions

| Name | Summary |
|---|---|
| [bestWebURL](best-web-u-r-l.md) | `fun bestWebURL(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Return best Web URL. |

### Companion Object Functions

| Name | Summary |
|---|---|
| [isWebURL](is-web-u-r-l.md) | `fun isWebURL(string: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Check if string is a Web URL. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
