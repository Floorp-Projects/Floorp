---
title: SystemEngine - 
---

[mozilla.components.browser.engine.system](../index.html) / [SystemEngine](./index.html)

# SystemEngine

`class SystemEngine : Engine`

WebView-based implementation of the Engine interface.

### Constructors

| [&lt;init&gt;](-init-.html) | `SystemEngine(defaultSettings: DefaultSettings? = null)`<br>WebView-based implementation of the Engine interface. |

### Properties

| [settings](settings.html) | `val settings: Settings`<br>See [Engine.settings](#) |

### Functions

| [createSession](create-session.html) | `fun createSession(private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): EngineSession`<br>Creates a new WebView-based EngineSession implementation. |
| [createView](create-view.html) | `fun createView(context: Context, attrs: AttributeSet?): EngineView`<br>Creates a new WebView-based EngineView implementation. |
| [name](name.html) | `fun name(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>See [Engine.name](#) |

