---
title: SessionIntentProcessor.<init> - 
---

[mozilla.components.feature.session](../index.html) / [SessionIntentProcessor](index.html) / [&lt;init&gt;](./-init-.html)

# &lt;init&gt;

`SessionIntentProcessor(sessionUseCases: `[`SessionUseCases`](../-session-use-cases/index.html)`, sessionManager: SessionManager, useDefaultHandlers: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, openNewTab: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true)`

Processor for intents which should trigger session-related actions.

### Parameters

`openNewTab` - Whether a processed Intent should open a new tab or open URLs in the currently
    selected tab.