---
title: FxaException - 
---

[mozilla.components.service.fxa](../index.html) / [FxaException](./index.html)

# FxaException

`open class FxaException : `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)

Wrapper class for the exceptions thrown in the Rust library, which ensures that the
error messages will be consumed and freed properly in Rust.

### Exceptions

| [Panic](-panic/index.html) | `class Panic : `[`FxaException`](./index.md) |
| [Unauthorized](-unauthorized/index.html) | `class Unauthorized : `[`FxaException`](./index.md) |
| [Unspecified](-unspecified/index.html) | `class Unspecified : `[`FxaException`](./index.md) |

### Constructors

| [&lt;init&gt;](-init-.html) | `FxaException(message: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>Wrapper class for the exceptions thrown in the Rust library, which ensures that the error messages will be consumed and freed properly in Rust. |

### Companion Object Functions

| [fromConsuming](from-consuming.html) | `fun fromConsuming(e: `[`Error`](../-error/index.html)`): `[`FxaException`](./index.md) |

### Inheritors

| [Panic](-panic/index.html) | `class Panic : `[`FxaException`](./index.md) |
| [Unauthorized](-unauthorized/index.html) | `class Unauthorized : `[`FxaException`](./index.md) |
| [Unspecified](-unspecified/index.html) | `class Unspecified : `[`FxaException`](./index.md) |

