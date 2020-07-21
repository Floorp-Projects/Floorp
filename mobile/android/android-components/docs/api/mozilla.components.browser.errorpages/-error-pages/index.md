[android-components](../../index.md) / [mozilla.components.browser.errorpages](../index.md) / [ErrorPages](./index.md)

# ErrorPages

`object ErrorPages` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/errorpages/src/main/java/mozilla/components/browser/errorpages/ErrorPages.kt#L12)

### Functions

| Name | Summary |
|---|---|
| [createErrorPage](create-error-page.md) | `fun createErrorPage(context: <ERROR CLASS>, errorType: `[`ErrorType`](../-error-type/index.md)`, uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, htmlResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = R.raw.error_pages, cssResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = R.raw.error_style): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Load and generate error page for the given error type and html/css resources. Encoded in Base64 &amp; does NOT support loading images |
| [createUrlEncodedErrorPage](create-url-encoded-error-page.md) | `fun createUrlEncodedErrorPage(context: <ERROR CLASS>, errorType: `[`ErrorType`](../-error-type/index.md)`, uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, htmlResource: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = HTML_RESOURCE_FILE): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Provides an encoded URL for an error page. Supports displaying images |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
