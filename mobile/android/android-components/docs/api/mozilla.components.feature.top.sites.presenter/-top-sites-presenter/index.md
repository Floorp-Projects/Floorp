[android-components](../../index.md) / [mozilla.components.feature.top.sites.presenter](../index.md) / [TopSitesPresenter](./index.md)

# TopSitesPresenter

`interface TopSitesPresenter : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/top-sites/src/main/java/mozilla/components/feature/top/sites/presenter/TopSitesPresenter.kt#L14)

A presenter that connects the [TopSitesView](../../mozilla.components.feature.top.sites.view/-top-sites-view/index.md) with the [TopSitesStorage](../../mozilla.components.feature.top.sites/-top-sites-storage/index.md).

### Properties

| Name | Summary |
|---|---|
| [storage](storage.md) | `abstract val storage: `[`TopSitesStorage`](../../mozilla.components.feature.top.sites/-top-sites-storage/index.md) |
| [view](view.md) | `abstract val view: `[`TopSitesView`](../../mozilla.components.feature.top.sites.view/-top-sites-view/index.md) |

### Inherited Functions

| Name | Summary |
|---|---|
| [start](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/start.md) | `abstract fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/stop.md) | `abstract fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
