[android-components](../../index.md) / [mozilla.components.support.base.crash](../index.md) / [Breadcrumb](./index.md)

# Breadcrumb

`data class Breadcrumb : `[`Comparable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-comparable/index.html)`<`[`Breadcrumb`](./index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/crash/Breadcrumb.kt#L19)

Represents a single crash breadcrumb.

### Types

| Name | Summary |
|---|---|
| [Level](-level/index.md) | `enum class Level`<br>Crash breadcrumb priority level. |
| [Type](-type/index.md) | `enum class Type`<br>Crash breadcrumb type. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Breadcrumb(message: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", data: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = emptyMap(), category: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", level: `[`Level`](-level/index.md)` = Level.DEBUG, type: `[`Type`](-type/index.md)` = Type.DEFAULT, date: `[`Date`](http://docs.oracle.com/javase/7/docs/api/java/util/Date.html)` = Date())`<br>Represents a single crash breadcrumb. |

### Properties

| Name | Summary |
|---|---|
| [category](category.md) | `val category: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Category of the crash breadcrumb. |
| [data](data.md) | `val data: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>Data related to the crash breadcrumb. |
| [date](date.md) | `val date: `[`Date`](http://docs.oracle.com/javase/7/docs/api/java/util/Date.html)<br>Date of of the crash breadcrumb. |
| [level](level.md) | `val level: `[`Level`](-level/index.md)<br>Level of the crash breadcrumb. |
| [message](message.md) | `val message: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Message of the crash breadcrumb. |
| [type](type.md) | `val type: `[`Type`](-type/index.md)<br>Type of the crash breadcrumb. |

### Functions

| Name | Summary |
|---|---|
| [compareTo](compare-to.md) | `fun compareTo(other: `[`Breadcrumb`](./index.md)`): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [toJson](to-json.md) | `fun toJson(): <ERROR CLASS>`<br>Converts Breadcrumb into a JSON object |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
