[android-components](../../index.md) / [org.mozilla.telemetry](../index.md) / [TelemetryHolder](./index.md)

# TelemetryHolder

`open class TelemetryHolder` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/TelemetryHolder.java#L16)

Holder of a static reference to the Telemetry instance. This is required for background services that somehow need to get access to the configuration and storage. This is not particular nice. Hopefully we can replace this with something better.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TelemetryHolder()`<br>Holder of a static reference to the Telemetry instance. This is required for background services that somehow need to get access to the configuration and storage. This is not particular nice. Hopefully we can replace this with something better. |

### Functions

| Name | Summary |
|---|---|
| [get](get.md) | `open static fun get(): `[`Telemetry`](../-telemetry/index.md) |
| [set](set.md) | `open static fun set(telemetry: `[`Telemetry`](../-telemetry/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
