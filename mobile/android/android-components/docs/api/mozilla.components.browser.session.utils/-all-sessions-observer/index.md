[android-components](../../index.md) / [mozilla.components.browser.session.utils](../index.md) / [AllSessionsObserver](./index.md)

# AllSessionsObserver

`class AllSessionsObserver` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/utils/AllSessionsObserver.kt#L16)

This class is a combination of [Session.Observer](../../mozilla.components.browser.session/-session/-observer/index.md) and [SessionManager.Observer](../../mozilla.components.browser.session/-session-manager/-observer/index.md). It observers all [Session](../../mozilla.components.browser.session/-session/index.md) instances
that get added to the [SessionManager](../../mozilla.components.browser.session/-session-manager/index.md) and automatically unsubscribes from [Session](../../mozilla.components.browser.session/-session/index.md) instances that get removed.

### Types

| Name | Summary |
|---|---|
| [Observer](-observer/index.md) | `interface Observer : `[`Observer`](../../mozilla.components.browser.session/-session/-observer/index.md) |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AllSessionsObserver(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, sessionObserver: `[`Observer`](-observer/index.md)`)`<br>This class is a combination of [Session.Observer](../../mozilla.components.browser.session/-session/-observer/index.md) and [SessionManager.Observer](../../mozilla.components.browser.session/-session-manager/-observer/index.md). It observers all [Session](../../mozilla.components.browser.session/-session/index.md) instances that get added to the [SessionManager](../../mozilla.components.browser.session/-session-manager/index.md) and automatically unsubscribes from [Session](../../mozilla.components.browser.session/-session/index.md) instances that get removed. |

### Functions

| Name | Summary |
|---|---|
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
