[android-components](../../index.md) / [mozilla.components.lib.jexl.ast](../index.md) / [AstNode](./index.md)

# AstNode

`data class AstNode` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/jexl/src/main/java/mozilla/components/lib/jexl/ast/AstNode.kt#L13)

A node of the abstract syntax tree.

This class has a lot of properties because it needs to represent all types of nodes (See [AstType](../-ast-type/index.md)). It should be
possible to make this much simpler by creating a dedicated class for each type.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AstNode(type: `[`AstType`](../-ast-type/index.md)`, value: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`? = null, operator: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, left: `[`AstNode`](./index.md)`? = null, right: `[`AstNode`](./index.md)`? = null, from: `[`AstNode`](./index.md)`? = null, relative: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, subject: `[`AstNode`](./index.md)`? = null, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, expression: `[`AstNode`](./index.md)`? = null, test: `[`AstNode`](./index.md)`? = null, consequent: `[`AstNode`](./index.md)`? = null, alternate: `[`AstNode`](./index.md)`? = null, arguments: `[`MutableList`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-mutable-list/index.html)`<`[`AstNode`](./index.md)`> = mutableListOf())`<br>A node of the abstract syntax tree. |

### Properties

| Name | Summary |
|---|---|
| [alternate](alternate.md) | `var alternate: `[`AstNode`](./index.md)`?` |
| [arguments](arguments.md) | `val arguments: `[`MutableList`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-mutable-list/index.html)`<`[`AstNode`](./index.md)`>` |
| [consequent](consequent.md) | `var consequent: `[`AstNode`](./index.md)`?` |
| [expression](expression.md) | `var expression: `[`AstNode`](./index.md)`?` |
| [from](from.md) | `var from: `[`AstNode`](./index.md)`?` |
| [left](left.md) | `var left: `[`AstNode`](./index.md)`?` |
| [name](name.md) | `var name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` |
| [operator](operator.md) | `val operator: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` |
| [relative](relative.md) | `var relative: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [right](right.md) | `var right: `[`AstNode`](./index.md)`?` |
| [subject](subject.md) | `var subject: `[`AstNode`](./index.md)`?` |
| [test](test.md) | `var test: `[`AstNode`](./index.md)`?` |
| [type](type.md) | `val type: `[`AstType`](../-ast-type/index.md) |
| [value](value.md) | `var value: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`?` |

### Functions

| Name | Summary |
|---|---|
| [equals](equals.md) | `fun equals(other: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [toString](to-string.md) | `fun toString(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
