[android-components](../../index.md) / [mozilla.components.service.experiments](../index.md) / [ValuesProvider](./index.md)

# ValuesProvider

`open class ValuesProvider` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/experiments/src/main/java/mozilla/components/service/experiments/ValuesProvider.kt#L16)

Class used to provide
custom filter values

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ValuesProvider()`<br>Class used to provide custom filter values |

### Functions

| Name | Summary |
|---|---|
| [getAppId](get-app-id.md) | `open fun getAppId(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Provides the app id (package name) |
| [getClientId](get-client-id.md) | `open fun getClientId(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Provides the client ID (UUID) used for bucketing the users. |
| [getCountry](get-country.md) | `open fun getCountry(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Provides the user's country |
| [getDevice](get-device.md) | `open fun getDevice(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Provides the device model |
| [getLanguage](get-language.md) | `open fun getLanguage(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Provides the user's language |
| [getManufacturer](get-manufacturer.md) | `open fun getManufacturer(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Provides the device manufacturer |
| [getRegion](get-region.md) | `open fun getRegion(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Provides the user's region |
| [getReleaseChannel](get-release-channel.md) | `open fun getReleaseChannel(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Provides the app's release channel (alpha, beta, ...) |
| [getVersion](get-version.md) | `open fun getVersion(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Provides the app version |
