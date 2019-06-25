[android-components](../../index.md) / [mozilla.components.browser.menu.item](../index.md) / [BrowserMenuCompoundButton](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`BrowserMenuCompoundButton(label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, initialState: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { false }, listener: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`

A browser menu compound button. A basic sub-class would only have to provide a layout resource to
satisfy [BrowserMenuItem.getLayoutResource](../../mozilla.components.browser.menu/-browser-menu-item/get-layout-resource.md) which contains a [View](#) that inherits from [CompoundButton](#).

### Parameters

`label` - The visible label of this menu item.

`initialState` - The initial value the checkbox should have.

`listener` - Callback to be invoked when this menu item is checked.