---
title: ErrorPages - 
---

[mozilla.components.browser.errorpages](../index.html) / [ErrorPages](./index.html)

# ErrorPages

`object ErrorPages`

### Types

| [ErrorType](-error-type/index.html) | `enum class ErrorType`<br>Enum containing all supported error types that we can display an error page for. |

### Functions

| [createErrorPage](create-error-page.html) | `fun createErrorPage(context: Context, errorType: `[`ErrorType`](-error-type/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Load and generate error page for the given error type. |
| [mapWebViewErrorCodeToErrorType](map-web-view-error-code-to-error-type.html) | `fun mapWebViewErrorCodeToErrorType(errorCode: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`ErrorType`](-error-type/index.html)<br>Map a WebView error code (WebViewClient.ERROR_*) to an element of the ErrorType enum. |

