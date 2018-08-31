---
title: DefaultSessionStorage.<init> - 
---

[mozilla.components.browser.session.storage](../index.html) / [DefaultSessionStorage](index.html) / [&lt;init&gt;](./-init-.html)

# &lt;init&gt;

`DefaultSessionStorage(context: Context, savePeriodically: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, saveIntervalInSeconds: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 300, scheduler: `[`ScheduledExecutorService`](http://docs.oracle.com/javase/6/docs/api/java/util/concurrent/ScheduledExecutorService.html)` = Executors.newSingleThreadScheduledExecutor())`

Default implementation of [SessionStorage](../-session-storage/index.html) which persists browser and engine
session states to a JSON file.

The JSON format used for persisting:
{
    "version": [version](#),
    "selectedSession": "[session-uuid](#)",
    "[session-uuid](#)": {
        "session": {}
        "engineSession": {}
    },
    "[session-uuid](#)": {
        "session": {}
        "engineSession": {}
    }
}

