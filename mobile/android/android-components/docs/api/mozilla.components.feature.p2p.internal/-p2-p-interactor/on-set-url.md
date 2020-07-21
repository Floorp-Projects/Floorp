[android-components](../../index.md) / [mozilla.components.feature.p2p.internal](../index.md) / [P2PInteractor](index.md) / [onSetUrl](./on-set-url.md)

# onSetUrl

`fun onSetUrl(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, newTab: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/p2p/src/main/java/mozilla/components/feature/p2p/internal/P2PInteractor.kt#L73)

Overrides [Listener.onSetUrl](../../mozilla.components.feature.p2p.view/-p2-p-view/-listener/on-set-url.md)

Handles a request to load the specified URL. This will typically be one sent from a neighbor.

### Parameters

`url` - the URL

`newTab` - whether to open the URL in a new tab (true) or the current one (false)