---
title: Config - 
---

[mozilla.components.service.fxa](../index.html) / [Config](./index.html)

# Config

`class Config : `[`RustObject`](../-rust-object/index.html)`<`[`RawConfig`](../-raw-config/index.html)`>`

Config represents the server endpoint configurations needed for the
authentication flow.

### Constructors

| [&lt;init&gt;](-init-.html) | `Config(rawPointer: `[`RawConfig`](../-raw-config/index.html)`?)`<br>Config represents the server endpoint configurations needed for the authentication flow. |

### Properties

| [rawPointer](raw-pointer.html) | `var rawPointer: `[`RawConfig`](../-raw-config/index.html)`?` |

### Inherited Properties

| [isConsumed](../-rust-object/is-consumed.html) | `val isConsumed: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Functions

| [destroy](destroy.html) | `fun destroy(p: `[`RawConfig`](../-raw-config/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inherited Functions

| [close](../-rust-object/close.html) | `open fun close(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [consumePointer](../-rust-object/consume-pointer.html) | `fun consumePointer(): `[`T`](../-rust-object/index.html#T) |
| [validPointer](../-rust-object/valid-pointer.html) | `fun validPointer(): `[`T`](../-rust-object/index.html#T) |

### Companion Object Functions

| [custom](custom.html) | `fun custom(content_base: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`FxaResult`](../-fxa-result/index.html)`<`[`Config`](./index.md)`>`<br>Set up endpoints used by a custom host for authentication |
| [release](release.html) | `fun release(): `[`FxaResult`](../-fxa-result/index.html)`<`[`Config`](./index.md)`>`<br>Set up endpoints used in the production Firefox Accounts instance. |

