[android-components](../../index.md) / [mozilla.components.support.utils](../index.md) / [SafeIntent](./index.md)

# SafeIntent

`class SafeIntent` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/utils/src/main/java/mozilla/components/support/utils/SafeIntent.kt#L20)

External applications can pass values into Intents that can cause us to crash: in defense,
we wrap [Intent](#) and catch the exceptions they may force us to throw. See bug 1090385
for more.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SafeIntent(unsafe: <ERROR CLASS>)`<br>External applications can pass values into Intents that can cause us to crash: in defense, we wrap [Intent](#) and catch the exceptions they may force us to throw. See bug 1090385 for more. |

### Properties

| Name | Summary |
|---|---|
| [action](action.md) | `val action: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` |
| [categories](categories.md) | `val categories: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>?` |
| [data](data.md) | `val data: <ERROR CLASS>?` |
| [dataString](data-string.md) | `val dataString: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` |
| [extras](extras.md) | `val extras: <ERROR CLASS>?` |
| [flags](flags.md) | `val flags: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [isLauncherIntent](is-launcher-intent.md) | `val isLauncherIntent: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [unsafe](unsafe.md) | `val unsafe: <ERROR CLASS>` |

### Functions

| Name | Summary |
|---|---|
| [getBooleanExtra](get-boolean-extra.md) | `fun getBooleanExtra(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, defaultValue: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [getBundleExtra](get-bundle-extra.md) | `fun getBundleExtra(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`SafeBundle`](../-safe-bundle/index.md)`?` |
| [getCharSequenceExtra](get-char-sequence-extra.md) | `fun getCharSequenceExtra(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`CharSequence`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char-sequence/index.html)`?` |
| [getIntExtra](get-int-extra.md) | `fun getIntExtra(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, defaultValue: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [getParcelableArrayListExtra](get-parcelable-array-list-extra.md) | `fun <T> getParcelableArrayListExtra(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`ArrayList`](https://developer.android.com/reference/java/util/ArrayList.html)`<`[`T`](get-parcelable-array-list-extra.md#T)`>?` |
| [getParcelableExtra](get-parcelable-extra.md) | `fun <T> getParcelableExtra(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`T`](get-parcelable-extra.md#T)`?` |
| [getStringArrayListExtra](get-string-array-list-extra.md) | `fun getStringArrayListExtra(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`ArrayList`](https://developer.android.com/reference/java/util/ArrayList.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>?` |
| [getStringExtra](get-string-extra.md) | `fun getStringExtra(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` |
| [hasExtra](has-extra.md) | `fun hasExtra(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [getSessionId](../../mozilla.components.feature.intent.ext/get-session-id.md) | `fun `[`SafeIntent`](./index.md)`.getSessionId(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Retrieves [mozilla.components.browser.session.Session](../../mozilla.components.browser.session/-session/index.md) ID from the intent. |
