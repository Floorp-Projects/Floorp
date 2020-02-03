[android-components](../../index.md) / [mozilla.components.feature.pwa](../index.md) / [ManifestStorage](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`ManifestStorage(context: <ERROR CLASS>, usedAtTimeout: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = DEFAULT_TIMEOUT)`

Disk storage for [WebAppManifest](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md). Other components use this class to reload a saved manifest.

### Parameters

`context` - the application context this storage is associated with

`usedAtTimeout` - a timeout in milliseconds after which the storage will consider a manifest
    as unused. By default this is 30 days.