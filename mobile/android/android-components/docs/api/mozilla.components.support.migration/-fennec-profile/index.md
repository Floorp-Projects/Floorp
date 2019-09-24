[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [FennecProfile](./index.md)

# FennecProfile

`data class FennecProfile` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecProfile.kt#L19)

A profile of "Fennec" (Firefox for Android).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FennecProfile(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, default: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`)`<br>A profile of "Fennec" (Firefox for Android). |

### Properties

| Name | Summary |
|---|---|
| [default](default.md) | `val default: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [name](name.md) | `val name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [path](path.md) | `val path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Companion Object Functions

| Name | Summary |
|---|---|
| [findDefault](find-default.md) | `fun findDefault(context: <ERROR CLASS>, mozillaDirectory: `[`File`](https://developer.android.com/reference/java/io/File.html)` = getMozillaDirectory(context), fileName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "profiles.ini"): `[`FennecProfile`](./index.md)`?`<br>Returns the default [FennecProfile](./index.md) - the default profile used by Fennec or `null` if no profile could be found. |
