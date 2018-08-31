---
title: ReloadPageAction - 
---

[org.mozilla.samples.toolbar](../index.html) / [ReloadPageAction](./index.html)

# ReloadPageAction

`class ReloadPageAction : Button`

A custom page action that either shows a reload button or a stop button based on the provided
isLoading lambda.

### Constructors

| [&lt;init&gt;](-init-.html) | `ReloadPageAction(reloadImageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, reloadContentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, stopImageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, stopContentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, isLoading: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, background: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`? = null, listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>A custom page action that either shows a reload button or a stop button based on the provided isLoading lambda. |

### Properties

| [loading](loading.html) | `var loading: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Functions

| [bind](bind.html) | `fun bind(view: View): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

