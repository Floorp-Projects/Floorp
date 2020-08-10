[android-components](../../index.md) / [mozilla.components.feature.p2p.internal](../index.md) / [P2PInteractor](index.md) / [onAccept](./on-accept.md)

# onAccept

`fun onAccept(token: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/p2p/src/main/java/mozilla/components/feature/p2p/internal/P2PInteractor.kt#L61)

Overrides [Listener.onAccept](../../mozilla.components.feature.p2p.view/-p2-p-view/-listener/on-accept.md)

Handles a decision to accept the connection specified by the given token. The value
of the token is what was passed to [authenticate](../../mozilla.components.feature.p2p.view/-p2-p-view/authenticate.md).

### Parameters

`token` - a short string uniquely identifying a connection between two devices