[android-components](../../index.md) / [mozilla.components.browser.icons](../index.md) / [BrowserIcons](./index.md)

# BrowserIcons

`class BrowserIcons : `[`MemoryConsumer`](../../mozilla.components.support.base.memory/-memory-consumer/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/BrowserIcons.kt#L77)

Entry point for loading icons for websites.

### Parameters

`generator` - The [IconGenerator](../../mozilla.components.browser.icons.generator/-icon-generator/index.md) to generate an icon if no icon could be loaded.

`decoders` - List of [IconDecoder](#) instances to use when decoding a loaded icon into a [android.graphics.Bitmap](#).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserIcons(context: <ERROR CLASS>, httpClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, generator: `[`IconGenerator`](../../mozilla.components.browser.icons.generator/-icon-generator/index.md)` = DefaultIconGenerator(), preparers: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`IconPreprarer`](../../mozilla.components.browser.icons.preparer/-icon-preprarer/index.md)`> = listOf(
        TippyTopIconPreparer(context.assets),
        MemoryIconPreparer(sharedMemoryCache),
        DiskIconPreparer(sharedDiskCache)
    ), loaders: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`IconLoader`](../../mozilla.components.browser.icons.loader/-icon-loader/index.md)`> = listOf(
        MemoryIconLoader(sharedMemoryCache),
        DiskIconLoader(sharedDiskCache),
        HttpIconLoader(httpClient),
        DataUriIconLoader()
    ), decoders: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`ImageDecoder`](../../mozilla.components.support.images.decoder/-image-decoder/index.md)`> = listOf(
        AndroidImageDecoder(),
        ICOIconDecoder()
    ), processors: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`IconProcessor`](../../mozilla.components.browser.icons.processor/-icon-processor/index.md)`> = listOf(
        MemoryIconProcessor(sharedMemoryCache),
        DiskIconProcessor(sharedDiskCache)
    ), jobDispatcher: CoroutineDispatcher = Executors.newFixedThreadPool(THREADS).asCoroutineDispatcher())`<br>Entry point for loading icons for websites. |

### Functions

| Name | Summary |
|---|---|
| [install](install.md) | `fun install(engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Installs the "icons" extension in the engine in order to dynamically load icons for loaded websites. |
| [loadIcon](load-icon.md) | `fun loadIcon(request: `[`IconRequest`](../-icon-request/index.md)`): Deferred<`[`Icon`](../-icon/index.md)`>`<br>Asynchronously loads an [Icon](../-icon/index.md) for the given [IconRequest](../-icon-request/index.md). |
| [loadIntoView](load-into-view.md) | `fun loadIntoView(view: <ERROR CLASS>, request: `[`IconRequest`](../-icon-request/index.md)`, placeholder: <ERROR CLASS>? = null, error: <ERROR CLASS>? = null): Job`<br>Loads an icon asynchronously using [BrowserIcons](./index.md) and then displays it in the [ImageView](#). If the view is detached from the window before loading is completed, then loading is cancelled. |
| [onLowMemory](on-low-memory.md) | `fun ~~onLowMemory~~(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>The device is running low on memory. This component should trim its memory usage. |
| [onTrimMemory](on-trim-memory.md) | `fun onTrimMemory(level: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies this component that it should try to release memory. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
