[android-components](../../index.md) / [mozilla.components.browser.icons](../index.md) / [BrowserIcons](./index.md)

# BrowserIcons

`class BrowserIcons` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/BrowserIcons.kt#L28)

Entry point for loading icons for websites.

### Parameters

`generator` - The [IconGenerator](../../mozilla.components.browser.icons.generator/-icon-generator/index.md) to generate an icon if no icon could be loaded.

`decoders` - List of [IconDecoder](../../mozilla.components.browser.icons.decoder/-icon-decoder/index.md) instances to use when decoding a loaded icon into a [android.graphics.Bitmap](https://developer.android.com/reference/android/graphics/Bitmap.html).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserIcons(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, generator: `[`IconGenerator`](../../mozilla.components.browser.icons.generator/-icon-generator/index.md)` = DefaultIconGenerator(context), decoders: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`IconDecoder`](../../mozilla.components.browser.icons.decoder/-icon-decoder/index.md)`> = listOf(
        ICOIconDecoder()
    ), jobDispatcher: CoroutineDispatcher = Executors.newFixedThreadPool(THREADS).asCoroutineDispatcher())`<br>Entry point for loading icons for websites. |

### Functions

| Name | Summary |
|---|---|
| [install](install.md) | `fun install(engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Installs the "icons" extension in the engine in order to dynamically load icons for loaded websites. |
| [loadIcon](load-icon.md) | `fun loadIcon(request: `[`IconRequest`](../-icon-request/index.md)`): Deferred<`[`Icon`](../-icon/index.md)`>`<br>Asynchronously loads an [Icon](../-icon/index.md) for the given [IconRequest](../-icon-request/index.md). |
