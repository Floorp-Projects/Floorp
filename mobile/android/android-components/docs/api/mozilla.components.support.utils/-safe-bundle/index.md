[android-components](../../index.md) / [mozilla.components.support.utils](../index.md) / [SafeBundle](./index.md)

# SafeBundle

`class SafeBundle` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/utils/src/main/java/mozilla/components/support/utils/SafeBundle.kt#L18)

See SafeIntent for more background: applications can put garbage values into Bundles. This is primarily
experienced when there's garbage in the Intent's Bundle. However that Bundle can contain further bundles,
and we need to handle those defensively too.

See bug 1090385 for more.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SafeBundle(unsafe: <ERROR CLASS>)`<br>See SafeIntent for more background: applications can put garbage values into Bundles. This is primarily experienced when there's garbage in the Intent's Bundle. However that Bundle can contain further bundles, and we need to handle those defensively too. |

### Properties

| Name | Summary |
|---|---|
| [unsafe](unsafe.md) | `val unsafe: <ERROR CLASS>` |

### Functions

| Name | Summary |
|---|---|
| [getInt](get-int.md) | `fun getInt(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, defaultValue: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [getParcelable](get-parcelable.md) | `fun <T> getParcelable(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`T`](get-parcelable.md#T)`?` |
| [getString](get-string.md) | `fun getString(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` |
