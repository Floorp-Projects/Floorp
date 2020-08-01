[android-components](../../index.md) / [mozilla.components.feature.p2p.internal](../index.md) / [P2PPresenter](./index.md)

# P2PPresenter

`class P2PPresenter` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/p2p/src/main/java/mozilla/components/feature/p2p/internal/P2PPresenter.kt#L21)

Sends updates to [P2PView](../../mozilla.components.feature.p2p.view/-p2-p-view/index.md) based on updates from [NearbyConnection](../../mozilla.components.lib.nearby/-nearby-connection/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `P2PPresenter(connectionProvider: () -> `[`NearbyConnection`](../../mozilla.components.lib.nearby/-nearby-connection/index.md)`, view: `[`P2PView`](../../mozilla.components.feature.p2p.view/-p2-p-view/index.md)`, outgoingMessages: `[`ConcurrentMap`](http://docs.oracle.com/javase/7/docs/api/java/util/concurrent/ConcurrentMap.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, `[`Char`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char/index.html)`>)`<br>Sends updates to [P2PView](../../mozilla.components.feature.p2p.view/-p2-p-view/index.md) based on updates from [NearbyConnection](../../mozilla.components.lib.nearby/-nearby-connection/index.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
