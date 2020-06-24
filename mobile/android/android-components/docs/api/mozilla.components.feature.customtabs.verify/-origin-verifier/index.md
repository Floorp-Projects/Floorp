[android-components](../../index.md) / [mozilla.components.feature.customtabs.verify](../index.md) / [OriginVerifier](./index.md)

# OriginVerifier

`class OriginVerifier` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/customtabs/src/main/java/mozilla/components/feature/customtabs/verify/OriginVerifier.kt#L29)

Used to verify postMessage origin for a designated package name.

Uses Digital Asset Links to confirm that the given origin is associated with the package name.
It caches any origin that has been verified during the current application
lifecycle and reuses that without making any new network requests.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `OriginVerifier(packageName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, relation: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, packageManager: <ERROR CLASS>, relationChecker: `[`RelationChecker`](../../mozilla.components.service.digitalassetlinks/-relation-checker/index.md)`)`<br>Used to verify postMessage origin for a designated package name. |

### Functions

| Name | Summary |
|---|---|
| [verifyOrigin](verify-origin.md) | `suspend fun verifyOrigin(origin: <ERROR CLASS>): <ERROR CLASS>`<br>Verify the claimed origin for the cached package name asynchronously. This will end up making a network request for non-cached origins with a HTTP [Client](../../mozilla.components.concept.fetch/-client/index.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
