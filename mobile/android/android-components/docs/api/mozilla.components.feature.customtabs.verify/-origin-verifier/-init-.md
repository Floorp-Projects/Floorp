[android-components](../../index.md) / [mozilla.components.feature.customtabs.verify](../index.md) / [OriginVerifier](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`OriginVerifier(packageName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, relation: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, packageManager: <ERROR CLASS>, httpClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`)`

Used to verify postMessage origin for a designated package name.

Uses Digital Asset Links to confirm that the given origin is associated with the package name.
It caches any origin that has been verified during the current application
lifecycle and reuses that without making any new network requests.

