[android-components](../../index.md) / [mozilla.components.lib.crash](../index.md) / [Breadcrumb](./index.md)

# Breadcrumb

`data class Breadcrumb : `[`Comparable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-comparable/index.html)`<`[`Breadcrumb`](./index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/Breadcrumb.kt#L15)

Represents a single crash breadcrumb.

### Types

| Name | Summary |
|---|---|
| [Level](-level/index.md) | `enum class Level`<br>Crash breadcrumb priority level. |
| [Type](-type/index.md) | `enum class Type`<br>Crash breadcrumb type. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Breadcrumb(message: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", data: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = emptyMap(), category: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", level: `[`Level`](-level/index.md)` = Level.DEBUG, type: `[`Type`](-type/index.md)` = Type.DEFAULT, date: `[`Date`](https://developer.android.com/reference/java/util/Date.html)` = Date())`<br>Represents a single crash breadcrumb. |

### Properties

| Name | Summary |
|---|---|
| [category](category.md) | `val category: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Category of the crash breadcrumb. |
| [data](data.md) | `val data: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>Data related to the crash breadcrumb. |
| [date](date.md) | `val date: `[`Date`](https://developer.android.com/reference/java/util/Date.html)<br>Date of of the crash breadcrumb. |
| [level](level.md) | `val level: `[`Level`](-level/index.md)<br>Level of the crash breadcrumb. |
| [message](message.md) | `val message: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Message of the crash breadcrumb. |
| [type](type.md) | `val type: `[`Type`](-type/index.md)<br>Type of the crash breadcrumb. |

### Functions

| Name | Summary |
|---|---|
| [compareTo](compare-to.md) | `fun compareTo(other: `[`Breadcrumb`](./index.md)`): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
