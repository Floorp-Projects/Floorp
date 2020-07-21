[android-components](../../index.md) / [mozilla.components.feature.p2p.view](../index.md) / [P2PBar](index.md) / [initializeButtons](./initialize-buttons.md)

# initializeButtons

`fun initializeButtons(connectB: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, sendB: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/p2p/src/main/java/mozilla/components/feature/p2p/view/P2PBar.kt#L59)

Overrides [P2PView.initializeButtons](../-p2-p-view/initialize-buttons.md)

This initializes the buttons of a [P2PView](../-p2-p-view/index.md) to restore the past button state.
It need not be called on the first [P2PView](../-p2-p-view/index.md) but should be called after one is destroyed
and another inflated. It is up to the implementation how the buttons should be presented
(such as whether they should be [View.GONE](#), [View.INVISIBLE](#), or just disabled when not
being presented. The reset button is enabled whenever the connection buttons are not.

### Parameters

`connectB` - whether the connection buttons (advertise and discover) should be presented

`sendB` - whether the send buttons (sendUrl and sendPage) should be presented