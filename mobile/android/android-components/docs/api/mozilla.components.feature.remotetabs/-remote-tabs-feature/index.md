[android-components](../../index.md) / [mozilla.components.feature.remotetabs](../index.md) / [RemoteTabsFeature](./index.md)

# RemoteTabsFeature

`@ExperimentalCoroutinesApi class RemoteTabsFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/remotetabs/src/main/java/mozilla/components/feature/remotetabs/RemoteTabsFeature.kt#L29)

A feature that listens to the [BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md) changes to update the local remote tabs state
in [RemoteTabsStorage](../../mozilla.components.browser.storage.sync/-remote-tabs-storage/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `RemoteTabsFeature(accountManager: `[`FxaAccountManager`](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md)`, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, tabsStorage: `[`RemoteTabsStorage`](../../mozilla.components.browser.storage.sync/-remote-tabs-storage/index.md)` = RemoteTabsStorage())`<br>A feature that listens to the [BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md) changes to update the local remote tabs state in [RemoteTabsStorage](../../mozilla.components.browser.storage.sync/-remote-tabs-storage/index.md). |

### Functions

| Name | Summary |
|---|---|
| [getRemoteTabs](get-remote-tabs.md) | `suspend fun getRemoteTabs(): `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`Device`](../../mozilla.components.concept.sync/-device/index.md)`, `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Tab`](../../mozilla.components.browser.storage.sync/-tab/index.md)`>>`<br>Get the list of remote tabs. |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
