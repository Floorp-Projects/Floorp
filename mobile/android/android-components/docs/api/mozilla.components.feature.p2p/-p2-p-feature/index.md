[android-components](../../index.md) / [mozilla.components.feature.p2p](../index.md) / [P2PFeature](./index.md)

# P2PFeature

`class P2PFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, `[`PermissionsFeature`](../../mozilla.components.support.base.feature/-permissions-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/p2p/src/main/java/mozilla/components/feature/p2p/P2PFeature.kt#L36)

Feature implementation for peer-to-peer communication between browsers.

### Types

| Name | Summary |
|---|---|
| [P2PFeatureSender](-p2-p-feature-sender/index.md) | `inner class P2PFeatureSender`<br>A class able to request an encoding of the current web page. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `P2PFeature(view: `[`P2PView`](../../mozilla.components.feature.p2p.view/-p2-p-view/index.md)`, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, connectionProvider: () -> `[`NearbyConnection`](../../mozilla.components.lib.nearby/-nearby-connection/index.md)`, tabsUseCases: `[`TabsUseCases`](../../mozilla.components.feature.tabs/-tabs-use-cases/index.md)`, sessionUseCases: `[`SessionUseCases`](../../mozilla.components.feature.session/-session-use-cases/index.md)`, onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../../mozilla.components.support.base.feature/-on-need-to-request-permissions.md)`, onClose: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>Feature implementation for peer-to-peer communication between browsers. |

### Properties

| Name | Summary |
|---|---|
| [onNeedToRequestPermissions](on-need-to-request-permissions.md) | `val onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../../mozilla.components.support.base.feature/-on-need-to-request-permissions.md)<br>A callback invoked when permissions need to be requested by the feature before it can complete its task. Once the request is completed, [onPermissionsResult](../../mozilla.components.support.base.feature/-permissions-feature/on-permissions-result.md) needs to be invoked. |

### Functions

| Name | Summary |
|---|---|
| [onPermissionsResult](on-permissions-result.md) | `fun onPermissionsResult(permissions: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, grantResults: `[`IntArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int-array/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies the feature that a permission request was completed. The feature should react to this and complete its task. |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
