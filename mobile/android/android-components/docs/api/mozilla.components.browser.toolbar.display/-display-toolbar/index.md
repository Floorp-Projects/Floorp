[android-components](../../index.md) / [mozilla.components.browser.toolbar.display](../index.md) / [DisplayToolbar](./index.md)

# DisplayToolbar

`class DisplayToolbar` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/toolbar/src/main/java/mozilla/components/browser/toolbar/display/DisplayToolbar.kt#L69)

Sub-component of the browser toolbar responsible for displaying the URL and related controls ("display mode").

Structure:

```
+-------------+------------+-----------------------+----------+------+
| navigation  | indicators | url       [ page    ] | browser  | menu |
|   actions   |            |           [ actions ] | actions  |      |
+-------------+------------+-----------------------+----------+------+
+------------------------progress------------------------------------+
```

Navigation actions (optional):
    A dynamic list of clickable icons usually used for navigation on larger devices
    (e.g. “back”/”forward” buttons.)

Indicators (optional):
    Tracking protection indicator icon (e.g. “shield” icon) that may show a doorhanger when clicked.
    Separator icon: a vertical line that separate the above and below icons.
    Site security indicator icon (e.g. “Lock” icon) that may show a doorhanger when clicked.
    Empty indicator: Icon that will be displayed if the URL is empty.

URL:
    Section that displays the current URL (read-only)

Page actions (optional):
    A dynamic list of clickable icons inside the URL section (e.g. “reader mode” icon)

Browser actions (optional):
    A list of dynamic clickable icons on the toolbar (e.g. tabs tray button)

Menu (optional):
    A button that shows an overflow menu when clicked (constructed using the browser-menu
    component)

Progress (optional):
    A horizontal progress bar showing the loading progress (at the top or bottom of the toolbar).

### Types

| Name | Summary |
|---|---|
| [Colors](-colors/index.md) | `data class Colors`<br>Data class holding the customizable colors in "display mode". |
| [Gravity](-gravity/index.md) | `enum class Gravity`<br>Gravity enum for positioning the progress bar. |
| [Icons](-icons/index.md) | `data class Icons`<br>Data class holding the customizable icons in "display mode". |
| [Indicators](-indicators/index.md) | `enum class Indicators`<br>Enum of indicators that can be displayed in the toolbar. |

### Properties

| Name | Summary |
|---|---|
| [colors](colors.md) | `var colors: `[`Colors`](-colors/index.md)<br>Customizable colors in "display mode". |
| [displayIndicatorSeparator](display-indicator-separator.md) | `var displayIndicatorSeparator: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [hint](hint.md) | `var hint: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Sets the text to be displayed when the URL of the toolbar is empty. |
| [icons](icons.md) | `var icons: `[`Icons`](-icons/index.md)<br>Customizable icons in "edit mode". |
| [indicators](indicators.md) | `var indicators: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Indicators`](-indicators/index.md)`>`<br>List of indicators that should be displayed next to the URL. |
| [menuBuilder](menu-builder.md) | `var menuBuilder: `[`BrowserMenuBuilder`](../../mozilla.components.browser.menu/-browser-menu-builder/index.md)`?`<br>Sets a [BrowserMenuBuilder](../../mozilla.components.browser.menu/-browser-menu-builder/index.md) that will be used to create a menu when the menu button is clicked. The menu button will only be visible if a builder or controller has been set. |
| [menuController](menu-controller.md) | `var menuController: `[`MenuController`](../../mozilla.components.concept.menu/-menu-controller/index.md)`?`<br>Sets a [MenuController](../../mozilla.components.concept.menu/-menu-controller/index.md) that will be used to create a menu when the menu button is clicked. The menu button will only be visible if a builder or controller has been set. If both a [menuBuilder](menu-builder.md) and controller are present, only the controller will be used. |
| [onUrlClicked](on-url-clicked.md) | `var onUrlClicked: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Sets a lambda that will be invoked whenever the URL in display mode was clicked. Only if this lambda returns true the toolbar will switch to editing mode. Return false to not switch to editing mode and handle the click manually. |
| [progressGravity](progress-gravity.md) | `var progressGravity: `[`Gravity`](-gravity/index.md)<br>Whether the progress bar should be drawn at the top or bottom of the toolbar. |
| [textSize](text-size.md) | `var textSize: `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html)<br>Sets the size of the text for the URL/search term displayed in the toolbar. |
| [titleTextSize](title-text-size.md) | `var titleTextSize: `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html)<br>Sets the size of the text for the title displayed in the toolbar. |
| [typeface](typeface.md) | `var typeface: <ERROR CLASS>`<br>Sets the typeface of the text for the URL/search term displayed in the toolbar. |
| [urlFormatter](url-formatter.md) | `var urlFormatter: (`[`CharSequence`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char-sequence/index.html)`) -> `[`CharSequence`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char-sequence/index.html)<br>Allows customization of URL for display purposes. |

### Functions

| Name | Summary |
|---|---|
| [setMenuDismissAction](set-menu-dismiss-action.md) | `fun setMenuDismissAction(onDismiss: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets a lambda to be invoked when the menu is dismissed |
| [setOnSiteSecurityClickedListener](set-on-site-security-clicked-listener.md) | `fun setOnSiteSecurityClickedListener(listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets a listener to be invoked when the site security indicator icon is clicked. |
| [setOnTrackingProtectionClickedListener](set-on-tracking-protection-clicked-listener.md) | `fun setOnTrackingProtectionClickedListener(listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets a listener to be invoked when the site tracking protection indicator icon is clicked. |
| [setOnUrlLongClickListener](set-on-url-long-click-listener.md) | `fun setOnUrlLongClickListener(handler: (<ERROR CLASS>) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Set a LongClickListener to the urlView of the toolbar. |
| [setUrlBackground](set-url-background.md) | `fun setUrlBackground(background: <ERROR CLASS>?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the background that should be drawn behind the URL, page actions an indicators. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
