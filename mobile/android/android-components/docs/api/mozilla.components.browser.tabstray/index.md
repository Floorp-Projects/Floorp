[android-components](../index.md) / [mozilla.components.browser.tabstray](./index.md)

## Package mozilla.components.browser.tabstray

### Types

| Name | Summary |
|---|---|
| [BrowserTabsTray](-browser-tabs-tray/index.md) | `class BrowserTabsTray : RecyclerView, `[`TabsTray`](../mozilla.components.concept.tabstray/-tabs-tray/index.md)<br>A customizable tabs tray for browsers. |
| [TabTouchCallback](-tab-touch-callback/index.md) | `open class TabTouchCallback : SimpleCallback`<br>An [ItemTouchHelper.Callback](#) for support gestures on tabs in the tray. |
| [TabViewHolder](-tab-view-holder/index.md) | `class TabViewHolder : ViewHolder, `[`Observer`](../mozilla.components.browser.session/-session/-observer/index.md)<br>A RecyclerView ViewHolder implementation for "tab" items. |
| [TabsAdapter](-tabs-adapter/index.md) | `class TabsAdapter : Adapter<`[`TabViewHolder`](-tab-view-holder/index.md)`>, `[`TabsTray`](../mozilla.components.concept.tabstray/-tabs-tray/index.md)`, `[`Observable`](../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](../mozilla.components.concept.tabstray/-tabs-tray/-observer/index.md)`>`<br>RecyclerView adapter implementation to display a list/grid of tabs. |

### Properties

| Name | Summary |
|---|---|
| [DEFAULT_ITEM_BACKGROUND_COLOR](-d-e-f-a-u-l-t_-i-t-e-m_-b-a-c-k-g-r-o-u-n-d_-c-o-l-o-r.md) | `const val DEFAULT_ITEM_BACKGROUND_COLOR: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [DEFAULT_ITEM_BACKGROUND_SELECTED_COLOR](-d-e-f-a-u-l-t_-i-t-e-m_-b-a-c-k-g-r-o-u-n-d_-s-e-l-e-c-t-e-d_-c-o-l-o-r.md) | `const val DEFAULT_ITEM_BACKGROUND_SELECTED_COLOR: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [DEFAULT_ITEM_TEXT_COLOR](-d-e-f-a-u-l-t_-i-t-e-m_-t-e-x-t_-c-o-l-o-r.md) | `const val DEFAULT_ITEM_TEXT_COLOR: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [DEFAULT_ITEM_TEXT_SELECTED_COLOR](-d-e-f-a-u-l-t_-i-t-e-m_-t-e-x-t_-s-e-l-e-c-t-e-d_-c-o-l-o-r.md) | `const val DEFAULT_ITEM_TEXT_SELECTED_COLOR: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
