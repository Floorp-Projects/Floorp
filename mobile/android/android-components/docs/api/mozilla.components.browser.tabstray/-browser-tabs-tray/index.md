[android-components](../../index.md) / [mozilla.components.browser.tabstray](../index.md) / [BrowserTabsTray](./index.md)

# BrowserTabsTray

`class ~~BrowserTabsTray~~ : RecyclerView, `[`TabsTray`](../../mozilla.components.concept.tabstray/-tabs-tray/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/tabstray/src/main/java/mozilla/components/browser/tabstray/BrowserTabsTray.kt#L20)
**Deprecated:** Use a RecyclerView directly instead; styling can be passed to the TabsAdapter. This class will be removed in a future release.

A customizable tabs tray for browsers.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserTabsTray(context: <ERROR CLASS>, attrs: <ERROR CLASS>? = null, defStyleAttr: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0, tabsAdapter: `[`TabsAdapter`](../-tabs-adapter/index.md)` = TabsAdapter(), layout: LayoutManager = GridLayoutManager(context, 2), itemDecoration: DividerItemDecoration? = null)`<br>A customizable tabs tray for browsers. |

### Properties

| Name | Summary |
|---|---|
| [tabsAdapter](tabs-adapter.md) | `val tabsAdapter: `[`TabsAdapter`](../-tabs-adapter/index.md) |

### Functions

| Name | Summary |
|---|---|
| [asView](as-view.md) | `fun asView(): <ERROR CLASS>`<br>Convenience method to cast this object to an Android View object. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
