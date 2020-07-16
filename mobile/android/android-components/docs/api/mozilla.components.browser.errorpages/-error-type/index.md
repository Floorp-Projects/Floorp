[android-components](../../index.md) / [mozilla.components.browser.errorpages](../index.md) / [ErrorType](./index.md)

# ErrorType

`enum class ErrorType` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/errorpages/src/main/java/mozilla/components/browser/errorpages/ErrorPages.kt#L116)

Enum containing all supported error types that we can display an error page for.

### Enum Values

| Name | Summary |
|---|---|
| [UNKNOWN](-u-n-k-n-o-w-n.md) |  |
| [ERROR_SECURITY_SSL](-e-r-r-o-r_-s-e-c-u-r-i-t-y_-s-s-l.md) |  |
| [ERROR_SECURITY_BAD_CERT](-e-r-r-o-r_-s-e-c-u-r-i-t-y_-b-a-d_-c-e-r-t.md) |  |
| [ERROR_NET_INTERRUPT](-e-r-r-o-r_-n-e-t_-i-n-t-e-r-r-u-p-t.md) |  |
| [ERROR_NET_TIMEOUT](-e-r-r-o-r_-n-e-t_-t-i-m-e-o-u-t.md) |  |
| [ERROR_CONNECTION_REFUSED](-e-r-r-o-r_-c-o-n-n-e-c-t-i-o-n_-r-e-f-u-s-e-d.md) |  |
| [ERROR_UNKNOWN_SOCKET_TYPE](-e-r-r-o-r_-u-n-k-n-o-w-n_-s-o-c-k-e-t_-t-y-p-e.md) |  |
| [ERROR_REDIRECT_LOOP](-e-r-r-o-r_-r-e-d-i-r-e-c-t_-l-o-o-p.md) |  |
| [ERROR_OFFLINE](-e-r-r-o-r_-o-f-f-l-i-n-e.md) |  |
| [ERROR_PORT_BLOCKED](-e-r-r-o-r_-p-o-r-t_-b-l-o-c-k-e-d.md) |  |
| [ERROR_NET_RESET](-e-r-r-o-r_-n-e-t_-r-e-s-e-t.md) |  |
| [ERROR_UNSAFE_CONTENT_TYPE](-e-r-r-o-r_-u-n-s-a-f-e_-c-o-n-t-e-n-t_-t-y-p-e.md) |  |
| [ERROR_CORRUPTED_CONTENT](-e-r-r-o-r_-c-o-r-r-u-p-t-e-d_-c-o-n-t-e-n-t.md) |  |
| [ERROR_CONTENT_CRASHED](-e-r-r-o-r_-c-o-n-t-e-n-t_-c-r-a-s-h-e-d.md) |  |
| [ERROR_INVALID_CONTENT_ENCODING](-e-r-r-o-r_-i-n-v-a-l-i-d_-c-o-n-t-e-n-t_-e-n-c-o-d-i-n-g.md) |  |
| [ERROR_UNKNOWN_HOST](-e-r-r-o-r_-u-n-k-n-o-w-n_-h-o-s-t.md) |  |
| [ERROR_NO_INTERNET](-e-r-r-o-r_-n-o_-i-n-t-e-r-n-e-t.md) |  |
| [ERROR_MALFORMED_URI](-e-r-r-o-r_-m-a-l-f-o-r-m-e-d_-u-r-i.md) |  |
| [ERROR_UNKNOWN_PROTOCOL](-e-r-r-o-r_-u-n-k-n-o-w-n_-p-r-o-t-o-c-o-l.md) |  |
| [ERROR_FILE_NOT_FOUND](-e-r-r-o-r_-f-i-l-e_-n-o-t_-f-o-u-n-d.md) |  |
| [ERROR_FILE_ACCESS_DENIED](-e-r-r-o-r_-f-i-l-e_-a-c-c-e-s-s_-d-e-n-i-e-d.md) |  |
| [ERROR_PROXY_CONNECTION_REFUSED](-e-r-r-o-r_-p-r-o-x-y_-c-o-n-n-e-c-t-i-o-n_-r-e-f-u-s-e-d.md) |  |
| [ERROR_UNKNOWN_PROXY_HOST](-e-r-r-o-r_-u-n-k-n-o-w-n_-p-r-o-x-y_-h-o-s-t.md) |  |
| [ERROR_SAFEBROWSING_MALWARE_URI](-e-r-r-o-r_-s-a-f-e-b-r-o-w-s-i-n-g_-m-a-l-w-a-r-e_-u-r-i.md) |  |
| [ERROR_SAFEBROWSING_UNWANTED_URI](-e-r-r-o-r_-s-a-f-e-b-r-o-w-s-i-n-g_-u-n-w-a-n-t-e-d_-u-r-i.md) |  |
| [ERROR_SAFEBROWSING_HARMFUL_URI](-e-r-r-o-r_-s-a-f-e-b-r-o-w-s-i-n-g_-h-a-r-m-f-u-l_-u-r-i.md) |  |
| [ERROR_SAFEBROWSING_PHISHING_URI](-e-r-r-o-r_-s-a-f-e-b-r-o-w-s-i-n-g_-p-h-i-s-h-i-n-g_-u-r-i.md) |  |

### Properties

| Name | Summary |
|---|---|
| [imageNameRes](image-name-res.md) | `val imageNameRes: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?` |
| [messageRes](message-res.md) | `val messageRes: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [refreshButtonRes](refresh-button-res.md) | `val refreshButtonRes: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [titleRes](title-res.md) | `val titleRes: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
