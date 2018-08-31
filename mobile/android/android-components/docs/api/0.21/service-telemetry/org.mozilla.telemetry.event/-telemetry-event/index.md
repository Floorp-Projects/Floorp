---
title: TelemetryEvent - 
---

[org.mozilla.telemetry.event](../index.html) / [TelemetryEvent](./index.html)

# TelemetryEvent

`open class TelemetryEvent`

TelemetryEvent specifies a common events data format, which allows for broader, shared usage of data processing tools.

### Functions

| [create](create.html) | `open static fun create(category: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, method: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, object: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`TelemetryEvent`](./index.md)<br>Create a new event with mandatory category, method and object.`open static fun create(category: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, method: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, object: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, value: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`TelemetryEvent`](./index.md)<br>Create a new event with mandatory category, method, object and value. |
| [extra](extra.html) | `open fun extra(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, value: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`TelemetryEvent`](./index.md) |
| [queue](queue.html) | `open fun queue(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Queue this event to be sent with the next event ping. |
| [toJSON](to-j-s-o-n.html) | `open fun toJSON(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Create a JSON representation of this event for storing and sending it. |

