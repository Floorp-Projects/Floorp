---
title: mozilla.components.concept.engine - 
---

[mozilla.components.concept.engine](./index.html)

## Package mozilla.components.concept.engine

### Types

| [DefaultSettings](-default-settings/index.html) | `data class DefaultSettings : `[`Settings`](-settings/index.html)<br>[Settings](-settings/index.html) implementation used to set defaults for [Engine](-engine/index.html) and [EngineSession](-engine-session/index.html). |
| [Engine](-engine/index.html) | `interface Engine`<br>Entry point for interacting with the engine implementation. |
| [EngineSession](-engine-session/index.html) | `abstract class EngineSession : Observable<`[`Observer`](-engine-session/-observer/index.html)`>`<br>Class representing a single engine session. |
| [EngineView](-engine-view/index.html) | `interface EngineView`<br>View component that renders web content. |
| [HitResult](-hit-result/index.html) | `sealed class HitResult`<br>Represents all the different supported types of data that can be found from long clicking an element. |
| [Settings](-settings/index.html) | `interface Settings`<br>Holds settings of an engine or session. Concrete engine implementations define how these settings are applied i.e. whether a setting is applied on an engine or session instance. |

### Exceptions

| [UnsupportedSettingException](-unsupported-setting-exception/index.html) | `class UnsupportedSettingException : `[`RuntimeException`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-runtime-exception/index.html)<br>Exception thrown by default if a setting is not supported by an engine or session. |

