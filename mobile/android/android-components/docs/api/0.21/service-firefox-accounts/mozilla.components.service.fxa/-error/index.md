---
title: Error - 
---

[mozilla.components.service.fxa](../index.html) / [Error](./index.html)

# Error

`open class Error : Structure`

### Types

| [ByReference](-by-reference/index.html) | `class ByReference : `[`Error`](./index.md)`, ByReference` |

### Constructors

| [&lt;init&gt;](-init-.html) | `Error()` |

### Properties

| [code](code.html) | `var code: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [message](message.html) | `var message: Pointer?` |

### Functions

| [consumeMessage](consume-message.html) | `fun consumeMessage(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Get and consume the error message, or null if there is none. |
| [getFieldOrder](get-field-order.html) | `open fun getFieldOrder(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` |
| [getMessage](get-message.html) | `fun getMessage(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Get the error message or null if there is none. |
| [isFailure](is-failure.html) | `fun isFailure(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Does this represent failure? |
| [isSuccess](is-success.html) | `fun isSuccess(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Does this represent success? |

### Inheritors

| [ByReference](-by-reference/index.html) | `class ByReference : `[`Error`](./index.md)`, ByReference` |

