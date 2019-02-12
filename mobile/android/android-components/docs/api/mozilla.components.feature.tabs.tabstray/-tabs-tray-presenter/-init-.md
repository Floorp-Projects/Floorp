[android-components](../../index.md) / [mozilla.components.feature.tabs.tabstray](../index.md) / [TabsTrayPresenter](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`TabsTrayPresenter(tabsTray: `[`TabsTray`](../../mozilla.components.concept.tabstray/-tabs-tray/index.md)`, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, closeTabsTray: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, sessionsFilter: (`[`Session`](../../mozilla.components.browser.session/-session/index.md)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true })`

Presenter implementation for a tabs tray implementation in order to update the tabs tray whenever
the state of the session manager changes.

