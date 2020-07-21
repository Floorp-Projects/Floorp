[android-components](../../index.md) / [mozilla.components.browser.toolbar](../index.md) / [BrowserToolbar](./index.md)

# BrowserToolbar

`class BrowserToolbar : `[`Toolbar`](../../mozilla.components.concept.toolbar/-toolbar/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/toolbar/src/main/java/mozilla/components/browser/toolbar/BrowserToolbar.kt#L60)

A customizable toolbar for browsers.

The toolbar can switch between two modes: display and edit. The display mode displays the current
URL and controls for navigation. In edit mode the current URL can be edited. Those two modes are
implemented by the DisplayToolbar and EditToolbar classes.

```
             +----------------+
             | BrowserToolbar |
             +--------+-------+
                      +
              +-------+-------+
              |               |
    +---------v------+ +-------v--------+
    | DisplayToolbar | |   EditToolbar  |
    +----------------+ +----------------+
```

### Types

| Name | Summary |
|---|---|
| [Button](-button/index.md) | `class Button : `[`ActionButton`](../../mozilla.components.concept.toolbar/-toolbar/-action-button/index.md)<br>An action button to be added to the toolbar. |
| [ToggleButton](-toggle-button/index.md) | `class ToggleButton : `[`ActionToggleButton`](../../mozilla.components.concept.toolbar/-toolbar/-action-toggle-button/index.md)<br>An action button with two states, selected and unselected. When the button is pressed, the state changes automatically. |
| [TwoStateButton](-two-state-button/index.md) | `class TwoStateButton : `[`Button`](-button/index.md)<br>An action that either shows an active button or an inactive button based on the provided isEnabled lambda. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserToolbar(context: <ERROR CLASS>, attrs: <ERROR CLASS>? = null, defStyleAttr: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0)`<br>A customizable toolbar for browsers. |

### Properties

| Name | Summary |
|---|---|
| [display](display.md) | `var display: `[`DisplayToolbar`](../../mozilla.components.browser.toolbar.display/-display-toolbar/index.md)<br>Toolbar in "display mode". |
| [edit](edit.md) | `var edit: `[`EditToolbar`](../../mozilla.components.browser.toolbar.edit/-edit-toolbar/index.md)<br>Toolbar in "edit mode". |
| [private](private.md) | `var private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Sets/gets private mode. |
| [siteSecure](site-secure.md) | `var siteSecure: `[`SiteSecurity`](../../mozilla.components.concept.toolbar/-toolbar/-site-security/index.md)<br>Sets/Gets the site security to be displayed on the toolbar. |
| [siteTrackingProtection](site-tracking-protection.md) | `var siteTrackingProtection: `[`SiteTrackingProtection`](../../mozilla.components.concept.toolbar/-toolbar/-site-tracking-protection/index.md)<br>Sets/Gets the site tracking protection state to be displayed on the toolbar. |
| [title](title.md) | `var title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Sets/Gets the title to be displayed on the toolbar. |
| [url](url.md) | `var url: `[`CharSequence`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char-sequence/index.html)<br>Sets/Gets the URL to be displayed on the toolbar. |

### Functions

| Name | Summary |
|---|---|
| [addBrowserAction](add-browser-action.md) | `fun addBrowserAction(action: `[`Action`](../../mozilla.components.concept.toolbar/-toolbar/-action/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds an action to be displayed on the right side of the toolbar (outside of the URL bounding box) in display mode. |
| [addEditAction](add-edit-action.md) | `fun addEditAction(action: `[`Action`](../../mozilla.components.concept.toolbar/-toolbar/-action/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds an action to be displayed on the right of the URL in edit mode. |
| [addNavigationAction](add-navigation-action.md) | `fun addNavigationAction(action: `[`Action`](../../mozilla.components.concept.toolbar/-toolbar/-action/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds an action to be display on the far left side of the toolbar. This area is usually used on larger devices for navigation actions like "back" and "forward". |
| [addPageAction](add-page-action.md) | `fun addPageAction(action: `[`Action`](../../mozilla.components.concept.toolbar/-toolbar/-action/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds an action to be displayed on the right side of the URL in display mode. |
| [displayMode](display-mode.md) | `fun displayMode(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Switches to URL displaying mode. |
| [displayProgress](display-progress.md) | `fun displayProgress(progress: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Displays the given loading progress. Expects values in the range [0,100](#). |
| [editMode](edit-mode.md) | `fun editMode(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Switches to URL editing mode. |
| [invalidateActions](invalidate-actions.md) | `fun invalidateActions(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Declare that the actions (navigation actions, browser actions, page actions) have changed and should be updated if needed. |
| [onBackPressed](on-back-pressed.md) | `fun onBackPressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Should be called by an activity when the user pressed the back key of the device. |
| [onLayout](on-layout.md) | `fun onLayout(changed: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, left: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, top: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, right: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, bottom: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onMeasure](on-measure.md) | `fun onMeasure(widthMeasureSpec: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, heightMeasureSpec: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onStop](on-stop.md) | `fun onStop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Should be called by the host activity when it enters the stop state. |
| [removeBrowserAction](remove-browser-action.md) | `fun removeBrowserAction(action: `[`Action`](../../mozilla.components.concept.toolbar/-toolbar/-action/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes a previously added browser action (see [addBrowserAction](add-browser-action.md)). If the provided action was never added, this method has no effect. |
| [removePageAction](remove-page-action.md) | `fun removePageAction(action: `[`Action`](../../mozilla.components.concept.toolbar/-toolbar/-action/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes a previously added page action (see [addPageAction](add-page-action.md)). If the provided action was never added, this method has no effect. |
| [setAutocompleteListener](set-autocomplete-listener.md) | `fun setAutocompleteListener(filter: suspend (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`AutocompleteDelegate`](../../mozilla.components.concept.toolbar/-autocomplete-delegate/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers the given function to be invoked when users changes text in the toolbar. |
| [setOnEditListener](set-on-edit-listener.md) | `fun setOnEditListener(listener: `[`OnEditListener`](../../mozilla.components.concept.toolbar/-toolbar/-on-edit-listener/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers the given listener to be invoked when the user edits the URL. |
| [setOnUrlCommitListener](set-on-url-commit-listener.md) | `fun setOnUrlCommitListener(listener: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers the given function to be invoked when the user selected a new URL i.e. is done editing. |
| [setSearchTerms](set-search-terms.md) | `fun setSearchTerms(searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Displays the currently used search terms as part of this Toolbar. |

### Inherited Functions

| Name | Summary |
|---|---|
| [asView](../../mozilla.components.concept.toolbar/-toolbar/as-view.md) | `open fun asView(): <ERROR CLASS>`<br>Casts this toolbar to an Android View object. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
