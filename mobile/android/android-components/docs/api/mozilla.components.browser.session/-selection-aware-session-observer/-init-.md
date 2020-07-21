[android-components](../../index.md) / [mozilla.components.browser.session](../index.md) / [SelectionAwareSessionObserver](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`SelectionAwareSessionObserver(sessionManager: `[`SessionManager`](../-session-manager/index.md)`)`

This class is a combination of [Session.Observer](../-session/-observer/index.md) and
[SessionManager.Observer](../-session-manager/-observer/index.md). It provides functionality to observe changes to a
specified or selected session, and can automatically take care of switching
over the observer in case a different session gets selected (see
[observeFixed](observe-fixed.md) and [observeSelected](observe-selected.md)).

