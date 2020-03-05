[android-components](../../index.md) / [mozilla.components.feature.tabs.tabstray](../index.md) / [TabsTrayPresenter](./index.md)

# TabsTrayPresenter

`class TabsTrayPresenter` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tabs/src/main/java/mozilla/components/feature/tabs/tabstray/TabsTrayPresenter.kt#L27)

Presenter implementation for a tabs tray implementation in order to update the tabs tray whenever
the state of the session manager changes.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TabsTrayPresenter(tabsTray: `[`TabsTray`](../../mozilla.components.concept.tabstray/-tabs-tray/index.md)`, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, tabsFilter: (`[`TabSessionState`](../../mozilla.components.browser.state.state/-tab-session-state/index.md)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, closeTabsTray: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>Presenter implementation for a tabs tray implementation in order to update the tabs tray whenever the state of the session manager changes. |

### Functions

| Name | Summary |
|---|---|
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
