---
title: BrowserToolbar - 
---

[mozilla.components.browser.toolbar](../index.html) / [BrowserToolbar](./index.html)

# BrowserToolbar

`class BrowserToolbar : ViewGroup, Toolbar`

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
```

+---------v------+ +-------v--------+
| DisplayToolbar | |   EditToolbar  |
+----------------+ +----------------+

### Types

| [Button](-button/index.html) | `class Button : ActionButton`<br>An action button to be added to the toolbar. |
| [ToggleButton](-toggle-button/index.html) | `class ToggleButton : ActionToggleButton`<br>An action button with two states, selected and unselected. When the button is pressed, the state changes automatically. |

### Constructors

| [&lt;init&gt;](-init-.html) | `BrowserToolbar(context: Context, attrs: AttributeSet? = null, defStyleAttr: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0)`<br>A customizable toolbar for browsers. |

### Properties

| [browserActionMargin](browser-action-margin.html) | `var browserActionMargin: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Gets/Sets the margin to be used between browser actions. |
| [displaySiteSecurityIcon](display-site-security-icon.html) | `var displaySiteSecurityIcon: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Set/Get whether a site security icon (usually a lock or globe icon) should be next to the URL. |
| [hint](hint.html) | `var hint: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Sets the text to be displayed when the URL of the toolbar is empty. |
| [onUrlClicked](on-url-clicked.html) | `var onUrlClicked: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Sets a lambda that will be invoked whenever the URL in display mode was clicked. Only if this lambda returns true the toolbar will switch to editing mode. Return false to not switch to editing mode and handle the click manually. |
| [url](url.html) | `var url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [urlBoxMargin](url-box-margin.html) | `var urlBoxMargin: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Gets/Sets horizontal margin of the URL box (surrounding URL and page actions) in display mode. |
| [urlBoxView](url-box-view.html) | `var urlBoxView: View?`<br>Gets/Sets a custom view that will be drawn as behind the URL and page actions in display mode. |

### Functions

| [addBrowserAction](add-browser-action.html) | `fun addBrowserAction(action: Action): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds an action to be displayed on the right side of the toolbar (outside of the URL bounding box) in display mode. |
| [addNavigationAction](add-navigation-action.html) | `fun addNavigationAction(action: Action): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds an action to be display on the far left side of the toolbar. This area is usually used on larger devices for navigation actions like "back" and "forward". |
| [addPageAction](add-page-action.html) | `fun addPageAction(action: Action): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds an action to be displayed on the right side of the URL in display mode. |
| [displayMode](display-mode.html) | `fun displayMode(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Switches to URL displaying mode. |
| [displayProgress](display-progress.html) | `fun displayProgress(progress: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [editMode](edit-mode.html) | `fun editMode(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Switches to URL editing mode. |
| [invalidateActions](invalidate-actions.html) | `fun invalidateActions(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Declare that the actions (navigation actions, browser actions, page actions) have changed and should be updated if needed. |
| [onBackPressed](on-back-pressed.html) | `fun onBackPressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onLayout](on-layout.html) | `fun onLayout(changed: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, left: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, top: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, right: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, bottom: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onMeasure](on-measure.html) | `fun onMeasure(widthMeasureSpec: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, heightMeasureSpec: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setAutocompleteFilter](set-autocomplete-filter.html) | `fun setAutocompleteFilter(filter: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, InlineAutocompleteEditText?) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets autocomplete filter to be used in edit mode. |
| [setMenuBuilder](set-menu-builder.html) | `fun setMenuBuilder(menuBuilder: BrowserMenuBuilder): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets a BrowserMenuBuilder that will be used to create a menu when the menu button is clicked. The menu button will only be visible if a builder has been set. |
| [setOnEditFocusChangeListener](set-on-edit-focus-change-listener.html) | `fun setOnEditFocusChangeListener(listener: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets a listener to be invoked when focus of the URL input view (in edit mode) changed. |
| [setOnUrlCommitListener](set-on-url-commit-listener.html) | `fun setOnUrlCommitListener(listener: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setSearchTerms](set-search-terms.html) | `fun setSearchTerms(searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setUrlTextPadding](set-url-text-padding.html) | `fun setUrlTextPadding(left: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = displayToolbar.urlView.paddingLeft, top: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = displayToolbar.urlView.paddingTop, right: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = displayToolbar.urlView.paddingRight, bottom: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = displayToolbar.urlView.paddingBottom): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the padding to be applied to the URL text (in display mode). |

