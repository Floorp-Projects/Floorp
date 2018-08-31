---
title: RustObject - 
---

[mozilla.components.service.fxa](../index.html) / [RustObject](./index.html)

# RustObject

`abstract class RustObject<T> : `[`Closeable`](http://docs.oracle.com/javase/6/docs/api/java/io/Closeable.html)

Base class that wraps an non-optional [Pointer](#) representing a pointer to a Rust object.
This class implements [Closeable](http://docs.oracle.com/javase/6/docs/api/java/io/Closeable.html) but does not provide an implementation, forcing all
subclasses to implement it. This ensures that all classes that inherit from RustObject
will have their [Pointer](#) destroyed when the Java wrapper is destroyed.

### Constructors

| [&lt;init&gt;](-init-.html) | `RustObject()`<br>Base class that wraps an non-optional [Pointer](#) representing a pointer to a Rust object. This class implements [Closeable](http://docs.oracle.com/javase/6/docs/api/java/io/Closeable.html) but does not provide an implementation, forcing all subclasses to implement it. This ensures that all classes that inherit from RustObject will have their [Pointer](#) destroyed when the Java wrapper is destroyed. |

### Properties

| [isConsumed](is-consumed.html) | `val isConsumed: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [rawPointer](raw-pointer.html) | `open var rawPointer: `[`T`](index.html#T)`?` |

### Functions

| [close](close.html) | `open fun close(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [consumePointer](consume-pointer.html) | `fun consumePointer(): `[`T`](index.html#T) |
| [destroy](destroy.html) | `abstract fun destroy(p: `[`T`](index.html#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [validPointer](valid-pointer.html) | `fun validPointer(): `[`T`](index.html#T) |

### Companion Object Functions

| [getAndConsumeString](get-and-consume-string.html) | `fun getAndConsumeString(stringPtr: Pointer?): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` |
| [safeAsync](safe-async.html) | `fun <U> safeAsync(callback: (`[`ByReference`](../-error/-by-reference/index.html)`) -> `[`U`](safe-async.html#U)`): `[`FxaResult`](../-fxa-result/index.html)`<`[`U`](safe-async.html#U)`>` |
| [safeSync](safe-sync.html) | `fun <U> safeSync(callback: (`[`ByReference`](../-error/-by-reference/index.html)`) -> `[`U`](safe-sync.html#U)`): `[`U`](safe-sync.html#U) |

### Inheritors

| [Config](../-config/index.html) | `class Config : `[`RustObject`](./index.md)`<`[`RawConfig`](../-raw-config/index.html)`>`<br>Config represents the server endpoint configurations needed for the authentication flow. |
| [FirefoxAccount](../-firefox-account/index.html) | `class FirefoxAccount : `[`RustObject`](./index.md)`<`[`RawFxAccount`](../-raw-fx-account/index.html)`>`<br>FirefoxAccount represents the authentication state of a client. |

