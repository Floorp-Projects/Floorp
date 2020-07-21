[android-components](../../index.md) / [org.mozilla.telemetry.serialize](../index.md) / [JSONPingSerializer](./index.md)

# JSONPingSerializer

`open class JSONPingSerializer : `[`TelemetryPingSerializer`](../-telemetry-ping-serializer/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/serialize/JSONPingSerializer.java#L16)

TelemetryPingSerializer that uses the org.json library provided by the Android system.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `JSONPingSerializer()`<br>TelemetryPingSerializer that uses the org.json library provided by the Android system. |

### Functions

| Name | Summary |
|---|---|
| [serialize](serialize.md) | `open fun serialize(ping: `[`TelemetryPing`](../../org.mozilla.telemetry.ping/-telemetry-ping/index.md)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
