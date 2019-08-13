[android-components](../../index.md) / [mozilla.components.browser.engine.gecko.glean](../index.md) / [GeckoAdapter](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`GeckoAdapter()`

This implements a [RuntimeTelemetry.Delegate](#) that dispatches Gecko runtime
telemetry to the Glean SDK.

Metrics defined in the `metrics.yaml` file in Gecko's mozilla-central repository
will be automatically dispatched to the Glean SDK and sent through the requested
pings.

This can be used, in products collecting data through the Glean SDK, by
providing an instance to `GeckoRuntimeSettings.Builder().telemetryDelegate`.

