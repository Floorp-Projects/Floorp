[android-components](../../../index.md) / [mozilla.components.support.migration](../../index.md) / [Result](../index.md) / [Failure](./index.md)

# Failure

`class Failure<T> : `[`Result`](../index.md)`<`[`T`](index.md#T)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/Result.kt#L25)

Failed migration.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Failure(throwable: `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`)``Failure(throwables: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`>)`<br>Failed migration. |

### Properties

| Name | Summary |
|---|---|
| [throwables](throwables.md) | `val throwables: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`>`<br>The [Throwable](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)s that caused this migration to fail. |
