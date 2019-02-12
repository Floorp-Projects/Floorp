[android-components](../../index.md) / [org.mozilla.telemetry.serialize](../index.md) / [TelemetryPingSerializer](./index.md)

# TelemetryPingSerializer

`interface TelemetryPingSerializer` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/serialize/TelemetryPingSerializer.java#L9)

### Functions

| Name | Summary |
|---|---|
| [serialize](serialize.md) | `abstract fun serialize(ping: `[`TelemetryPing`](../../org.mozilla.telemetry.ping/-telemetry-ping/index.md)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [JSONPingSerializer](../-j-s-o-n-ping-serializer/index.md) | `open class JSONPingSerializer : `[`TelemetryPingSerializer`](./index.md)<br>TelemetryPingSerializer that uses the org.json library provided by the Android system. |
