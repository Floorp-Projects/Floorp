[android-components](../../index.md) / [mozilla.components.service.glean.testing](../index.md) / [GleanTestRule](./index.md)

# GleanTestRule

`class GleanTestRule : TestWatcher` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/testing/GleanTestRule.kt#L33)

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

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `GleanTestRule(context: <ERROR CLASS>, configToUse: `[`Configuration`](../../mozilla.components.service.glean.config/-configuration/index.md)` = Configuration())`<br>This implements a JUnit rule for writing tests for Glean SDK metrics. |

### Properties

| Name | Summary |
|---|---|
| [configToUse](config-to-use.md) | `val configToUse: `[`Configuration`](../../mozilla.components.service.glean.config/-configuration/index.md)<br>an optional [Configuration](../../mozilla.components.service.glean.config/-configuration/index.md) to initialize the Glean SDK with |
| [context](context.md) | `val context: <ERROR CLASS>`<br>the application context |

### Functions

| Name | Summary |
|---|---|
| [starting](starting.md) | `fun starting(description: Description?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
