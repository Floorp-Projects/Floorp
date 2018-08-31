---
title: Settings - 
---

[mozilla.components.concept.engine](../index.html) / [Settings](./index.html)

# Settings

`interface Settings`

Holds settings of an engine or session. Concrete engine
implementations define how these settings are applied i.e.
whether a setting is applied on an engine or session instance.

### Properties

| [domStorageEnabled](dom-storage-enabled.html) | `open var domStorageEnabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Setting to control whether or not DOM Storage is enabled. |
| [javascriptEnabled](javascript-enabled.html) | `open var javascriptEnabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Setting to control whether or not JavaScript is enabled. |
| [requestInterceptor](request-interceptor.html) | `open var requestInterceptor: `[`RequestInterceptor`](../../mozilla.components.concept.engine.request/-request-interceptor/index.html)`?`<br>Setting to intercept and override requests. |
| [trackingProtectionPolicy](tracking-protection-policy.html) | `open var trackingProtectionPolicy: `[`TrackingProtectionPolicy`](../-engine-session/-tracking-protection-policy/index.html)`?`<br>Setting to control tracking protection. |

### Inheritors

| [DefaultSettings](../-default-settings/index.html) | `data class DefaultSettings : `[`Settings`](./index.md)<br>[Settings](./index.md) implementation used to set defaults for [Engine](../-engine/index.html) and [EngineSession](../-engine-session/index.html). |

