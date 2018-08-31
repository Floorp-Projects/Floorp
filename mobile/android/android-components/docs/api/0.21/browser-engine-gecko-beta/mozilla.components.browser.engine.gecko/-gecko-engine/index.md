---
title: GeckoEngine - 
---

[mozilla.components.browser.engine.gecko](../index.html) / [GeckoEngine](./index.html)

# GeckoEngine

`class GeckoEngine : Engine`

Gecko-based implementation of Engine interface.

### Constructors

| [&lt;init&gt;](-init-.html) | `GeckoEngine(runtime: GeckoRuntime, defaultSettings: Settings? = null)`<br>Gecko-based implementation of Engine interface. |

### Properties

| [settings](settings.html) | `val settings: Settings`<br>See [Engine.settings](#) |

### Functions

| [createSession](create-session.html) | `fun createSession(private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): EngineSession`<br>Creates a new Gecko-based EngineSession. |
| [createView](create-view.html) | `fun createView(context: Context, attrs: AttributeSet?): EngineView`<br>Creates a new Gecko-based EngineView. |
| [name](name.html) | `fun name(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

