[android-components](../../index.md) / [mozilla.components.browser.engine.servo](../index.md) / [ServoEngineSession](index.md) / [loadData](./load-data.md)

# loadData

`fun loadData(data: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, mimeType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, encoding: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-servo/src/main/java/mozilla/components/browser/engine/servo/ServoEngineSession.kt#L86)

Overrides [EngineSession.loadData](../../mozilla.components.concept.engine/-engine-session/load-data.md)

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