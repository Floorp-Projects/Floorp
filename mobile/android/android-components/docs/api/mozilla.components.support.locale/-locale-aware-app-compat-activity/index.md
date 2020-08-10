[android-components](../../index.md) / [mozilla.components.support.locale](../index.md) / [LocaleAwareAppCompatActivity](./index.md)

# LocaleAwareAppCompatActivity

`open class LocaleAwareAppCompatActivity : AppCompatActivity` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/locale/src/main/java/mozilla/components/support/locale/LocaleAwareAppCompatActivity.kt#L16)

Base activity for apps that want to customized the system defined language by their own.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `LocaleAwareAppCompatActivity()`<br>Base activity for apps that want to customized the system defined language by their own. |

### Functions

| Name | Summary |
|---|---|
| [attachBaseContext](attach-base-context.md) | `open fun attachBaseContext(base: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onCreate](on-create.md) | `open fun onCreate(savedInstanceState: <ERROR CLASS>?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setLayoutDirectionIfNeeded](set-layout-direction-if-needed.md) | `fun setLayoutDirectionIfNeeded(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Compensates for a bug in Android 8 which doesn't change the layoutDirection on activity recreation https://github.com/mozilla-mobile/fenix/issues/9413 https://stackoverflow.com/questions/46296202/rtl-layout-bug-in-android-oreo#comment98890942_46298101 |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
