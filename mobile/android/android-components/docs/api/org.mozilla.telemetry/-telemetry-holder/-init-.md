[android-components](../../index.md) / [org.mozilla.telemetry](../index.md) / [TelemetryHolder](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`TelemetryHolder()`
**Deprecated:** The whole service-telemetry library is deprecated. Please use the [Glean SDK](#) instead.

Holder of a static reference to the Telemetry instance. This is required for background services that somehow need to get access to the configuration and storage. This is not particular nice. Hopefully we can replace this with something better.

