[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [Action](./index.md)

# Action

`data class Action` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/Action.kt#L20)

Value type that represents the state of a browser or page action within a [WebExtension](../-web-extension/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Action(title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, enabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`?, loadIcon: suspend (`[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`) -> <ERROR CLASS>?, badgeText: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, badgeTextColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?, badgeBackgroundColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?, onClick: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>Value type that represents the state of a browser or page action within a [WebExtension](../-web-extension/index.md). |

### Properties

| Name | Summary |
|---|---|
| [badgeBackgroundColor](badge-background-color.md) | `val badgeBackgroundColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?`<br>The browser action's badge background color. |
| [badgeText](badge-text.md) | `val badgeText: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>The browser action's badge text. |
| [badgeTextColor](badge-text-color.md) | `val badgeTextColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?`<br>The browser action's badge text color. |
| [enabled](enabled.md) | `val enabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`?`<br>Indicates if the browser action should be enabled or disabled. |
| [loadIcon](load-icon.md) | `val loadIcon: suspend (`[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`) -> <ERROR CLASS>?`<br>A suspending function returning the icon in the provided size. |
| [onClick](on-click.md) | `val onClick: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A callback to be executed when this browser action is clicked. |
| [title](title.md) | `val title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>The title of the browser action to be visible in the user interface. |

### Functions

| Name | Summary |
|---|---|
| [copyWithOverride](copy-with-override.md) | `fun copyWithOverride(override: `[`Action`](./index.md)`): `[`Action`](./index.md)<br>Returns a copy of this [Action](./index.md) with the provided override applied e.g. for tab-specific overrides. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
