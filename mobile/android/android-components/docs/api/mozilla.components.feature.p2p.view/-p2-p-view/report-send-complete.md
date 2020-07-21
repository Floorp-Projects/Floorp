[android-components](../../index.md) / [mozilla.components.feature.p2p.view](../index.md) / [P2PView](index.md) / [reportSendComplete](./report-send-complete.md)

# reportSendComplete

`abstract fun reportSendComplete(resid: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/p2p/src/main/java/mozilla/components/feature/p2p/view/P2PView.kt#L89)

Indicates that the requested send operation is complete. The view may optionally display
the specified string.

### Parameters

`resid` - the resource if of a string confirming that the requested send was completed