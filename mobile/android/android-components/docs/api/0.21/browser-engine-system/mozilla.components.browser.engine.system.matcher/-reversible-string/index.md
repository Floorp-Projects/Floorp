---
title: ReversibleString - 
---

[mozilla.components.browser.engine.system.matcher](../index.html) / [ReversibleString](./index.html)

# ReversibleString

`abstract class ReversibleString`

A String wrapper utility that allows for efficient string reversal. We
regularly need to reverse strings. The standard way of doing this in Java
would be to copy the string to reverse (e.g. using StringBuffer.reverse()).
This seems wasteful when we only read our Strings character by character,
in which case can just transpose positions as needed.

### Properties

| [isReversed](is-reversed.html) | `abstract val isReversed: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [offsetEnd](offset-end.html) | `val offsetEnd: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [offsetStart](offset-start.html) | `val offsetStart: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [string](string.html) | `val string: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Functions

| [charAt](char-at.html) | `abstract fun charAt(position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Char`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char/index.html) |
| [length](length.html) | `fun length(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Returns the length of this string. |
| [reverse](reverse.html) | `fun reverse(): `[`ReversibleString`](./index.md)<br>Reverses this string. |
| [substring](substring.html) | `abstract fun substring(startIndex: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`ReversibleString`](./index.md) |

### Companion Object Functions

| [create](create.html) | `fun create(string: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`ReversibleString`](./index.md)<br>Create a [ReversibleString](./index.md) for the provided [String](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html). |

