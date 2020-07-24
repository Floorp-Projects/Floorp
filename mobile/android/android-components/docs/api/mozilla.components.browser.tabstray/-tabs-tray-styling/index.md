[android-components](../../index.md) / [mozilla.components.browser.tabstray](../index.md) / [TabsTrayStyling](./index.md)

# TabsTrayStyling

`data class TabsTrayStyling` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/tabstray/src/main/java/mozilla/components/browser/tabstray/TabsTrayStyling.kt#L24)

Tabs tray styling for items in the [TabsAdapter](../-tabs-adapter/index.md). If a custom [TabViewHolder](../-tab-view-holder/index.md)
is used with [TabsAdapter.viewHolderProvider](#), the styling can be applied
when [TabViewHolder.bind](../-tab-view-holder/bind.md) is invoked.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TabsTrayStyling(itemBackgroundColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = DEFAULT_ITEM_BACKGROUND_COLOR, selectedItemBackgroundColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = DEFAULT_ITEM_BACKGROUND_SELECTED_COLOR, itemTextColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = DEFAULT_ITEM_TEXT_COLOR, selectedItemTextColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = DEFAULT_ITEM_TEXT_SELECTED_COLOR, itemUrlTextColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = DEFAULT_ITEM_TEXT_COLOR, selectedItemUrlTextColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = DEFAULT_ITEM_TEXT_SELECTED_COLOR)`<br>Tabs tray styling for items in the [TabsAdapter](../-tabs-adapter/index.md). If a custom [TabViewHolder](../-tab-view-holder/index.md) is used with [TabsAdapter.viewHolderProvider](#), the styling can be applied when [TabViewHolder.bind](../-tab-view-holder/bind.md) is invoked. |

### Properties

| Name | Summary |
|---|---|
| [itemBackgroundColor](item-background-color.md) | `val itemBackgroundColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>the background color for all non-selected tabs. |
| [itemTextColor](item-text-color.md) | `val itemTextColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>the text color for all non-selected tabs. |
| [itemUrlTextColor](item-url-text-color.md) | `val itemUrlTextColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>the URL text color for all non-selected tabs. |
| [selectedItemBackgroundColor](selected-item-background-color.md) | `val selectedItemBackgroundColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>the background color for the selected tab. |
| [selectedItemTextColor](selected-item-text-color.md) | `val selectedItemTextColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>the text color for the selected tabs. |
| [selectedItemUrlTextColor](selected-item-url-text-color.md) | `val selectedItemUrlTextColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>the URL text color for the selected tab. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
