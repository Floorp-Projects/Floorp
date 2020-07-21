[android-components](../../index.md) / [mozilla.components.feature.p2p.internal](../index.md) / [P2PInteractor](index.md) / [onLoadData](./on-load-data.md)

# onLoadData

`fun onLoadData(context: <ERROR CLASS>, data: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, newTab: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/p2p/src/main/java/mozilla/components/feature/p2p/internal/P2PInteractor.kt#L120)

Overrides [Listener.onLoadData](../../mozilla.components.feature.p2p.view/-p2-p-view/-listener/on-load-data.md)

Loads the specified data into this tab.

### Parameters

`context` - the current context

`data` - the contents of the page

`newTab` - whether to open the URL in a new tab (true) or the current one (false)