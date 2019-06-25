[android-components](../../index.md) / [mozilla.components.service.fretboard](../index.md) / [ValuesProvider](./index.md)

# ValuesProvider

`open class ValuesProvider` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/fretboard/src/main/java/mozilla/components/service/fretboard/ValuesProvider.kt#L16)

Class used to provide
custom filter values

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ValuesProvider()`<br>Class used to provide custom filter values |

### Functions

| Name | Summary |
|---|---|
| [getAppId](get-app-id.md) | `open fun getAppId(context: <ERROR CLASS>): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Provides the app id (package name) |
| [getClientId](get-client-id.md) | `open fun getClientId(context: <ERROR CLASS>): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Provides the client ID (UUID) used for bucketing the users. |
| [getCountry](get-country.md) | `open fun getCountry(context: <ERROR CLASS>): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Provides the user's country |
| [getDevice](get-device.md) | `open fun getDevice(context: <ERROR CLASS>): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Provides the device model |
| [getLanguage](get-language.md) | `open fun getLanguage(context: <ERROR CLASS>): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Provides the user's language |
| [getManufacturer](get-manufacturer.md) | `open fun getManufacturer(context: <ERROR CLASS>): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Provides the device manufacturer |
| [getRegion](get-region.md) | `open fun getRegion(context: <ERROR CLASS>): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Provides the user's region |
| [getReleaseChannel](get-release-channel.md) | `open fun getReleaseChannel(context: <ERROR CLASS>): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Provides the app's release channel (alpha, beta, ...) |
| [getVersion](get-version.md) | `open fun getVersion(context: <ERROR CLASS>): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Provides the app version |
