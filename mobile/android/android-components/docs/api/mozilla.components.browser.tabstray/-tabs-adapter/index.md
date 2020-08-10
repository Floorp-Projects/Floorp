[android-components](../../index.md) / [mozilla.components.browser.tabstray](../index.md) / [TabsAdapter](./index.md)

# TabsAdapter

`open class TabsAdapter : Adapter<`[`TabViewHolder`](../-tab-view-holder/index.md)`>, `[`TabsTray`](../../mozilla.components.concept.tabstray/-tabs-tray/index.md)`, `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](../../mozilla.components.concept.tabstray/-tabs-tray/-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/tabstray/src/main/java/mozilla/components/browser/tabstray/TabsAdapter.kt#L29)

RecyclerView adapter implementation to display a list/grid of tabs.

### Parameters

`thumbnailLoader` - an implementation of an [ImageLoader](../../mozilla.components.support.images.loader/-image-loader/index.md) for loading thumbnail images in the tabs tray.

`viewHolderProvider` - a function that creates a `TabViewHolder`.

`delegate` - TabsTray.Observer registry to allow `TabsAdapter` to conform to `Observable<TabsTray.Observer>`.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TabsAdapter(thumbnailLoader: `[`ImageLoader`](../../mozilla.components.support.images.loader/-image-loader/index.md)`? = null, viewHolderProvider: `[`ViewHolderProvider`](../-view-holder-provider.md)` = { parent ->
        DefaultTabViewHolder(
            LayoutInflater.from(parent.context).inflate(R.layout.mozac_browser_tabstray_item, parent, false),
            thumbnailLoader
        )
    }, delegate: `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](../../mozilla.components.concept.tabstray/-tabs-tray/-observer/index.md)`> = ObserverRegistry())`<br>RecyclerView adapter implementation to display a list/grid of tabs. |

### Properties

| Name | Summary |
|---|---|
| [styling](styling.md) | `var styling: `[`TabsTrayStyling`](../-tabs-tray-styling/index.md) |

### Functions

| Name | Summary |
|---|---|
| [getItemCount](get-item-count.md) | `open fun getItemCount(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [onBindViewHolder](on-bind-view-holder.md) | `open fun onBindViewHolder(holder: `[`TabViewHolder`](../-tab-view-holder/index.md)`, position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onCreateViewHolder](on-create-view-holder.md) | `open fun onCreateViewHolder(parent: <ERROR CLASS>, viewType: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`TabViewHolder`](../-tab-view-holder/index.md) |
| [onTabsChanged](on-tabs-changed.md) | `open fun onTabsChanged(position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, count: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called after updateTabs() when count number of tabs are updated at the given position. |
| [onTabsInserted](on-tabs-inserted.md) | `open fun onTabsInserted(position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, count: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called after updateTabs() when count number of tabs are inserted at the given position. |
| [onTabsMoved](on-tabs-moved.md) | `open fun onTabsMoved(fromPosition: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, toPosition: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called after updateTabs() when a tab changes it position. |
| [onTabsRemoved](on-tabs-removed.md) | `open fun onTabsRemoved(position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, count: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called after updateTabs() when count number of tabs are removed from the given position. |
| [updateTabs](update-tabs.md) | `open fun updateTabs(tabs: `[`Tabs`](../../mozilla.components.concept.tabstray/-tabs/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Updates the list of tabs. |

### Inherited Functions

| Name | Summary |
|---|---|
| [asView](../../mozilla.components.concept.tabstray/-tabs-tray/as-view.md) | `open fun asView(): <ERROR CLASS>`<br>Convenience method to cast the implementation of this interface to an Android View object. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
