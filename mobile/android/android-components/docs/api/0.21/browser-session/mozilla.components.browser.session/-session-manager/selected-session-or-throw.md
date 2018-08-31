---
title: SessionManager.selectedSessionOrThrow - 
---

[mozilla.components.browser.session](../index.html) / [SessionManager](index.html) / [selectedSessionOrThrow](./selected-session-or-throw.html)

# selectedSessionOrThrow

`val selectedSessionOrThrow: `[`Session`](../-session/index.html)

Gets the currently selected session or throws an IllegalStateException if no session is
selected.

It's application specific whether a session manager can have no selected session (= no sessions)
or not. In applications that always have at least one session dealing with the nullable
selectedSession property can be cumbersome. In those situations, where you always
expect a session to exist, you can use selectedSessionOrThrow to avoid dealing
with null values.

Only one session can be selected at a given time.

