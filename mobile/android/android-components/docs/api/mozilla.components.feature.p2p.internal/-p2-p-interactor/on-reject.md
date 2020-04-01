[android-components](../../index.md) / [mozilla.components.feature.p2p.internal](../index.md) / [P2PInteractor](index.md) / [onReject](./on-reject.md)

# onReject

`fun onReject(token: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/p2p/src/main/java/mozilla/components/feature/p2p/internal/P2PInteractor.kt#L67)

Overrides [Listener.onReject](../../mozilla.components.feature.p2p.view/-p2-p-view/-listener/on-reject.md)

Handles a decision to reject the connection specified by the given token. The value
of the token is what was passed to [authenticate](../../mozilla.components.feature.p2p.view/-p2-p-view/authenticate.md).

### Parameters

`token` - a short string uniquely identifying a connection between two devices