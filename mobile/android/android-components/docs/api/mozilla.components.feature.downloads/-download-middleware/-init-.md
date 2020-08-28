[android-components](../../index.md) / [mozilla.components.feature.downloads](../index.md) / [DownloadMiddleware](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`DownloadMiddleware(applicationContext: <ERROR CLASS>, downloadServiceClass: `[`Class`](http://docs.oracle.com/javase/7/docs/api/java/lang/Class.html)`<*>, coroutineContext: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = Dispatchers.IO, downloadStorage: `[`DownloadStorage`](../-download-storage/index.md)` = DownloadStorage(applicationContext))`

[Middleware](../../mozilla.components.lib.state/-middleware.md) implementation for managing downloads via the provided download service. Its
purpose is to react to global download state changes (e.g. of [BrowserState.downloads](../../mozilla.components.browser.state.state/-browser-state/downloads.md))
and notify the download service, as needed.

