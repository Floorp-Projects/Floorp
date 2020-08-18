[android-components](../index.md) / [mozilla.components.support.ktx.android.net](./index.md)

## Package mozilla.components.support.ktx.android.net

### Properties

| Name | Summary |
|---|---|
| [hostWithoutCommonPrefixes](host-without-common-prefixes.md) | `val <ERROR CLASS>.hostWithoutCommonPrefixes: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Returns the host without common prefixes like "www" or "m". |
| [isHttpOrHttps](is-http-or-https.md) | `val <ERROR CLASS>.isHttpOrHttps: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if the [Uri](#) uses the "http" or "https" protocol scheme. |

### Functions

| Name | Summary |
|---|---|
| [getFileExtension](get-file-extension.md) | `fun <ERROR CLASS>.getFileExtension(contentResolver: <ERROR CLASS>): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Return a file extension for [this](get-file-extension/-this-.md) give Uri (only supports content:// schemes). |
| [getFileName](get-file-name.md) | `fun <ERROR CLASS>.getFileName(contentResolver: <ERROR CLASS>): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Return a file name for [this](get-file-name/-this-.md) give Uri. |
| [isInScope](is-in-scope.md) | `fun <ERROR CLASS>.isInScope(scopes: `[`Iterable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-iterable/index.html)`<<ERROR CLASS>>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks that the given URL is in one of the given URL [scopes](is-in-scope.md#mozilla.components.support.ktx.android.net$isInScope(, kotlin.collections.Iterable(()))/scopes). |
| [isUnderPrivateAppDirectory](is-under-private-app-directory.md) | `fun <ERROR CLASS>.isUnderPrivateAppDirectory(context: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Indicate if the [this](is-under-private-app-directory/-this-.md) uri is under the application private directory. |
| [sameOriginAs](same-origin-as.md) | `fun <ERROR CLASS>.sameOriginAs(other: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks that Uri has the same origin as [other](same-origin-as.md#mozilla.components.support.ktx.android.net$sameOriginAs(, )/other). |
| [sameSchemeAndHostAs](same-scheme-and-host-as.md) | `fun <ERROR CLASS>.sameSchemeAndHostAs(other: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks that Uri has the same scheme and host as [other](same-scheme-and-host-as.md#mozilla.components.support.ktx.android.net$sameSchemeAndHostAs(, )/other). |
