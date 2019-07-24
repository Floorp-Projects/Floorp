[android-components](../../index.md) / [mozilla.components.lib.state](../index.md) / [StoreException](./index.md)

# StoreException

`class StoreException : `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/state/src/main/java/mozilla/components/lib/state/Store.kt#L187)

Exception for otherwise unhandled errors caught while reducing state or
while managing/notifying observers.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `StoreException(msg: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, e: `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`? = null)`<br>Exception for otherwise unhandled errors caught while reducing state or while managing/notifying observers. |

### Properties

| Name | Summary |
|---|---|
| [e](e.md) | `val e: `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`?` |
| [msg](msg.md) | `val msg: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
