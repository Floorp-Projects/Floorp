[android-components](../../index.md) / [mozilla.components.feature.p2p.view](../index.md) / [P2PView](index.md) / [receiveUrl](./receive-url.md)

# receiveUrl

`abstract fun receiveUrl(neighborId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, neighborName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/p2p/src/main/java/mozilla/components/feature/p2p/view/P2PView.kt#L102)

Handles receipt of a URL from the specified neighbor. For example, the view could prompt the
user to accept the URL, upon which [Listener.onSetUrl](-listener/on-set-url.md) would be called. Only an internal
error would cause `neighborName` to be null. To be robust, a view should use
`neighborName ?: neighborId` as the name of the neighbor.

### Parameters

`neighborId` - the endpoint ID of the neighbor

`neighborName` - the name of the neighbor, which will only be null if there was
    an internal error in the connection library

`url` - the URL