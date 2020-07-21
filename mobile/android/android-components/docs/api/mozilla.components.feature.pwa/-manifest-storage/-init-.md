[android-components](../../index.md) / [mozilla.components.feature.pwa](../index.md) / [ManifestStorage](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`ManifestStorage(context: <ERROR CLASS>, activeThresholdMs: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = ACTIVE_THRESHOLD_MS)`

Disk storage for [WebAppManifest](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md). Other components use this class to reload a saved manifest.

### Parameters

`context` - the application context this storage is associated with

`activeThresholdMs` - a timeout in milliseconds after which the storage will consider a manifest
    as unused. By default this is [ACTIVE_THRESHOLD_MS](-a-c-t-i-v-e_-t-h-r-e-s-h-o-l-d_-m-s.md).