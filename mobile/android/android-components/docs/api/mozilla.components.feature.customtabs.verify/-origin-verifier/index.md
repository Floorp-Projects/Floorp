[android-components](../../index.md) / [mozilla.components.feature.customtabs.verify](../index.md) / [OriginVerifier](./index.md)

# OriginVerifier

`class OriginVerifier` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/customtabs/src/main/java/mozilla/components/feature/customtabs/verify/OriginVerifier.kt#L37)

Used to verify postMessage origin for a designated package name.

Uses Digital Asset Links to confirm that the given origin is associated with the package name as
a postMessage origin. It caches any origin that has been verified during the current application
lifecycle and reuses that without making any new network requests.

The lifecycle of this object is governed by the owner. The owner has to call
{@link OriginVerifier#cleanUp()} for proper cleanup of dependencies.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `OriginVerifier(packageName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, relation: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, packageManager: <ERROR CLASS>, httpClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`)`<br>Used to verify postMessage origin for a designated package name. |

### Functions

| Name | Summary |
|---|---|
| [verifyOrigin](verify-origin.md) | `suspend fun verifyOrigin(origin: <ERROR CLASS>): <ERROR CLASS>`<br>Verify the claimed origin for the cached package name asynchronously. This will end up making a network request for non-cached origins with a HTTP [Client](../../mozilla.components.concept.fetch/-client/index.md). |
