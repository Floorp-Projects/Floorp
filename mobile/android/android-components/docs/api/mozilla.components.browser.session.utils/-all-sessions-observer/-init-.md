[android-components](../../index.md) / [mozilla.components.browser.session.utils](../index.md) / [AllSessionsObserver](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`AllSessionsObserver(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, sessionObserver: `[`Observer`](-observer/index.md)`)`

This class is a combination of [Session.Observer](../../mozilla.components.browser.session/-session/-observer/index.md) and [SessionManager.Observer](../../mozilla.components.browser.session/-session-manager/-observer/index.md). It observers all [Session](../../mozilla.components.browser.session/-session/index.md) instances
that get added to the [SessionManager](../../mozilla.components.browser.session/-session-manager/index.md) and automatically unsubscribes from [Session](../../mozilla.components.browser.session/-session/index.md) instances that get removed.

