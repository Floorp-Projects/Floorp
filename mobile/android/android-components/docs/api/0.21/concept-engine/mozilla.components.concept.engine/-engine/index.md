---
title: Engine - 
---

[mozilla.components.concept.engine](../index.html) / [Engine](./index.html)

# Engine

`interface Engine`

Entry point for interacting with the engine implementation.

### Properties

| [settings](settings.html) | `abstract val settings: `[`Settings`](../-settings/index.html)<br>Provides access to the settings of this engine. |

### Functions

| [createSession](create-session.html) | `abstract fun createSession(private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`EngineSession`](../-engine-session/index.html)<br>Creates a new engine session. |
| [createView](create-view.html) | `abstract fun createView(context: Context, attrs: AttributeSet? = null): `[`EngineView`](../-engine-view/index.html)<br>Creates a new view for rendering web content. |
| [name](name.html) | `abstract fun name(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Returns the name of this engine. The returned string might be used in filenames and must therefore only contain valid filename characters. |

