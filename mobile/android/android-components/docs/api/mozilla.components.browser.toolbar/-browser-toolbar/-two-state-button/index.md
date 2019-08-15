[android-components](../../../index.md) / [mozilla.components.browser.toolbar](../../index.md) / [BrowserToolbar](../index.md) / [TwoStateButton](./index.md)

# TwoStateButton

`class TwoStateButton : `[`Button`](../-button/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/toolbar/src/main/java/mozilla/components/browser/toolbar/BrowserToolbar.kt#L771)

An action that either shows an active button or an inactive button based on the provided
isEnabled lambda.

### Parameters

`enabledImage` - The drawable to be show if the button is in the enabled stated.

`enabledContentDescription` - The content description to use if the button is in the enabled state.

`disabledImage` - The drawable to be show if the button is in the disabled stated.

`disabledContentDescription` - The content description to use if the button is in the enabled state.

`isEnabled` - Lambda that returns true of false to indicate whether this button should be enabled/disabled.

`background` - A custom (stateful) background drawable resource to be used.

`listener` - Callback that will be invoked whenever the checked state changes.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TwoStateButton(enabledImage: <ERROR CLASS>, enabledContentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, disabledImage: <ERROR CLASS>, disabledContentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, isEnabled: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true }, background: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0, listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>An action that either shows an active button or an inactive button based on the provided isEnabled lambda. |

### Properties

| Name | Summary |
|---|---|
| [enabled](enabled.md) | `var enabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Inherited Properties

| Name | Summary |
|---|---|
| [padding](../-button/padding.md) | `val padding: `[`Padding`](../../../mozilla.components.support.base.android/-padding/index.md)<br>a custom [Padding](../../../mozilla.components.support.base.android/-padding/index.md) for this Button. |

### Functions

| Name | Summary |
|---|---|
| [bind](bind.md) | `open fun bind(view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
