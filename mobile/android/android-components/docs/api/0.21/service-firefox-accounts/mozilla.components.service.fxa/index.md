---
title: mozilla.components.service.fxa - 
---

[mozilla.components.service.fxa](./index.html)

## Package mozilla.components.service.fxa

### Types

| [Config](-config/index.html) | `class Config : `[`RustObject`](-rust-object/index.html)`<`[`RawConfig`](-raw-config/index.html)`>`<br>Config represents the server endpoint configurations needed for the authentication flow. |
| [Error](-error/index.html) | `open class Error : Structure` |
| [FirefoxAccount](-firefox-account/index.html) | `class FirefoxAccount : `[`RustObject`](-rust-object/index.html)`<`[`RawFxAccount`](-raw-fx-account/index.html)`>`<br>FirefoxAccount represents the authentication state of a client. |
| [FxaResult](-fxa-result/index.html) | `class FxaResult<T>`<br>FxaResult is a class that represents an asynchronous result. |
| [OAuthInfo](-o-auth-info/index.html) | `class OAuthInfo` |
| [Profile](-profile/index.html) | `class Profile` |
| [RawConfig](-raw-config/index.html) | `class RawConfig : PointerType` |
| [RawFxAccount](-raw-fx-account/index.html) | `class RawFxAccount : PointerType` |
| [RustObject](-rust-object/index.html) | `abstract class RustObject<T> : `[`Closeable`](http://docs.oracle.com/javase/6/docs/api/java/io/Closeable.html)<br>Base class that wraps an non-optional [Pointer](#) representing a pointer to a Rust object. This class implements [Closeable](http://docs.oracle.com/javase/6/docs/api/java/io/Closeable.html) but does not provide an implementation, forcing all subclasses to implement it. This ensures that all classes that inherit from RustObject will have their [Pointer](#) destroyed when the Java wrapper is destroyed. |
| [SyncKeys](-sync-keys/index.html) | `class SyncKeys` |

### Exceptions

| [FxaException](-fxa-exception/index.html) | `open class FxaException : `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)<br>Wrapper class for the exceptions thrown in the Rust library, which ensures that the error messages will be consumed and freed properly in Rust. |

