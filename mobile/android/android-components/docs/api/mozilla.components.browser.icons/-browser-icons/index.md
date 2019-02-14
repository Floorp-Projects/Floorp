[android-components](../../index.md) / [mozilla.components.browser.icons](../index.md) / [BrowserIcons](./index.md)

# BrowserIcons

`class BrowserIcons` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/BrowserIcons.kt#L23)

Entry point for loading icons for websites.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserIcons(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, generator: `[`IconGenerator`](../../mozilla.components.browser.icons.generator/-icon-generator/index.md)` = DefaultIconGenerator(context), jobDispatcher: CoroutineDispatcher = Executors.newFixedThreadPool(THREADS).asCoroutineDispatcher())`<br>Entry point for loading icons for websites. |

### Functions

| Name | Summary |
|---|---|
| [loadIcon](load-icon.md) | `fun loadIcon(request: `[`IconRequest`](../-icon-request/index.md)`): Deferred<`[`Icon`](../-icon/index.md)`>`<br>Asynchronously loads an [Icon](../-icon/index.md) for the given [IconRequest](../-icon-request/index.md). |
