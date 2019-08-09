[android-components](../../index.md) / [mozilla.components.service.glean.testing](../index.md) / [GleanTestRule](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`GleanTestRule(context: <ERROR CLASS>, configToUse: `[`Configuration`](../../mozilla.components.service.glean.config/-configuration/index.md)` = Configuration())`

This implements a JUnit rule for writing tests for Glean SDK metrics.

The rule takes care of resetting the Glean SDK between tests and
initializing all the required dependencies.

Example usage:

```
// Add the following lines to you test class.
@get:Rule
val gleanRule = GleanTestRule(ApplicationProvider.getApplicationContext())
```

### Parameters

`context` - the application context

`configToUse` - an optional [Configuration](../../mozilla.components.service.glean.config/-configuration/index.md) to initialize the Glean SDK with