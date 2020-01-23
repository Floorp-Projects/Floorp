[android-components](../../index.md) / [mozilla.components.feature.toolbar](../index.md) / [WebExtensionToolbarAction](./index.md)

# WebExtensionToolbarAction

`open class WebExtensionToolbarAction : `[`Action`](../../mozilla.components.concept.toolbar/-toolbar/-action/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/toolbar/src/main/java/mozilla/components/feature/toolbar/WebExtensionToolbarAction.kt#L31)

An action button that represents an web extension item to be added to the toolbar.

### Parameters

`action` - Associated [WebExtensionBrowserAction](../../mozilla.components.concept.engine.webextension/-web-extension-browser-action.md)

`listener` - Callback that will be invoked whenever the button is pressed

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WebExtensionToolbarAction(action: `[`WebExtensionBrowserAction`](../../mozilla.components.concept.engine.webextension/-web-extension-browser-action.md)`, padding: `[`Padding`](../../mozilla.components.support.base.android/-padding/index.md)`? = null, iconJobDispatcher: CoroutineDispatcher, listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>An action button that represents an web extension item to be added to the toolbar. |

### Inherited Properties

| Name | Summary |
|---|---|
| [visible](../../mozilla.components.concept.toolbar/-toolbar/-action/visible.md) | `open val visible: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Functions

| Name | Summary |
|---|---|
| [bind](bind.md) | `open fun bind(view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [createView](create-view.md) | `open fun createView(parent: <ERROR CLASS>): <ERROR CLASS>` |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
