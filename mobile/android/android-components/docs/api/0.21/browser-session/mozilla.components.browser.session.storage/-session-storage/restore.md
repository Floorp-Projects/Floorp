---
title: SessionStorage.restore - 
---

[mozilla.components.browser.session.storage](../index.html) / [SessionStorage](index.html) / [restore](./restore.html)

# restore

`abstract fun restore(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)

Restores the session storage state by reading from the latest persisted version.

### Parameters

`sessionManager` - the session manager to restore into.

**Return**
map of all restored sessions, and the currently selected session id.

