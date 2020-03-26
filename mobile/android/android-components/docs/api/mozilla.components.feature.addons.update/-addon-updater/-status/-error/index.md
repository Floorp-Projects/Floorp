[android-components](../../../../index.md) / [mozilla.components.feature.addons.update](../../../index.md) / [AddonUpdater](../../index.md) / [Status](../index.md) / [Error](./index.md)

# Error

`data class Error : `[`Status`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/update/AddonUpdater.kt#L130)

An error has happened while trying to update.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Error(message: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, exception: `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`)`<br>An error has happened while trying to update. |

### Properties

| Name | Summary |
|---|---|
| [exception](exception.md) | `val exception: `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)<br>The exception of the error. |
| [message](message.md) | `val message: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>A string message describing what has happened. |
