[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [EngineSession](index.md) / [loadData](./load-data.md)

# loadData

`abstract fun loadData(data: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, mimeType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "text/html", encoding: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "UTF-8"): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSession.kt#L368)

Loads the data with the given mimeType.
Example:

```
engineSession.loadData("<html><body>Example HTML content here</body></html>", "text/html")
```

If the data is base64 encoded, you can override the default encoding (UTF-8) with 'base64'.
Example:

```
engineSession.loadData("ahr0cdovl21vemlsbgeub3jn==", "text/plain", "base64")
```

### Parameters

`data` - The data that should be rendering.

`mimeType` - the data type needed by the engine to know how to render it.

`encoding` - specifies whether the data is base64 encoded; use 'base64' else defaults to "UTF-8".