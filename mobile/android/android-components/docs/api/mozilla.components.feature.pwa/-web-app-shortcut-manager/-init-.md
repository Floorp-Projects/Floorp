[android-components](../../index.md) / [mozilla.components.feature.pwa](../index.md) / [WebAppShortcutManager](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`WebAppShortcutManager(context: <ERROR CLASS>, httpClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, storage: `[`ManifestStorage`](../-manifest-storage/index.md)`, supportWebApps: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true)`

Helper to manage pinned shortcuts for websites.

### Parameters

`httpClient` - Fetch client used to load website icons.

`storage` - Storage used to save web app manifests to disk.

`supportWebApps` - If true, Progressive Web Apps will be pinnable.
If false, all web sites will be bookmark shortcuts even if they have a manifest.