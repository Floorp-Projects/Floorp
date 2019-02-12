[android-components](../../index.md) / [mozilla.components.lib.jexl.ast](../index.md) / [AstNode](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`AstNode(type: `[`AstType`](../-ast-type/index.md)`, value: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`? = null, operator: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, left: `[`AstNode`](index.md)`? = null, right: `[`AstNode`](index.md)`? = null, from: `[`AstNode`](index.md)`? = null, relative: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, subject: `[`AstNode`](index.md)`? = null, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, expression: `[`AstNode`](index.md)`? = null, test: `[`AstNode`](index.md)`? = null, consequent: `[`AstNode`](index.md)`? = null, alternate: `[`AstNode`](index.md)`? = null, arguments: `[`MutableList`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-mutable-list/index.html)`<`[`AstNode`](index.md)`> = mutableListOf())`

A node of the abstract syntax tree.

This class has a lot of properties because it needs to represent all types of nodes (See [AstType](../-ast-type/index.md)). It should be
possible to make this much simpler by creating a dedicated class for each type.

