[android-components](../../index.md) / [mozilla.components.browser.icons](../index.md) / [BrowserIcons](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`BrowserIcons(context: <ERROR CLASS>, httpClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, generator: `[`IconGenerator`](../../mozilla.components.browser.icons.generator/-icon-generator/index.md)` = DefaultIconGenerator(), preparers: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`IconPreprarer`](../../mozilla.components.browser.icons.preparer/-icon-preprarer/index.md)`> = listOf(
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
    ), jobDispatcher: CoroutineDispatcher = Executors.newFixedThreadPool(THREADS).asCoroutineDispatcher())`

Entry point for loading icons for websites.

### Parameters

`generator` - The [IconGenerator](../../mozilla.components.browser.icons.generator/-icon-generator/index.md) to generate an icon if no icon could be loaded.

`decoders` - List of [IconDecoder](#) instances to use when decoding a loaded icon into a [android.graphics.Bitmap](#).