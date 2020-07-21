[android-components](../../index.md) / [mozilla.components.feature.p2p.view](../index.md) / [P2PBar](index.md) / [receivePage](./receive-page.md)

# receivePage

`fun receivePage(neighborId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, neighborName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, page: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/p2p/src/main/java/mozilla/components/feature/p2p/view/P2PBar.kt#L131)

Overrides [P2PView.receivePage](../-p2-p-view/receive-page.md)

Handles receipt of a web page from another device. Only an internal
error would cause `neighborName` to be null. To be robust, a view should use
`neighborName ?: neighborId` as the name of the neighbor.

### Parameters

`neighborId` - the endpoint ID of the neighbor

`neighborName` - the name of the neighbor, which will only be null if there was
    an internal error in the connection library

`page` - a representation of the web page