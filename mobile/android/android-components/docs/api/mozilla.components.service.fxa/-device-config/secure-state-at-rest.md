[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [DeviceConfig](index.md) / [secureStateAtRest](./secure-state-at-rest.md)

# secureStateAtRest

`val secureStateAtRest: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/Config.kt#L41)

A flag indicating whether or not to use encrypted storage for the persisted account
state. If set to `true`, [SecureAbove22AccountStorage](#) will be used as a storage layer. As the name suggests,
account state will only by encrypted on Android API 23+. Otherwise, even if this flag is set to `true`, account state
will be stored in plaintext.

Default value of `false` configures the plaintext version of account storage to be used, [SharedPrefAccountStorage](#).

Switching of this flag's values is supported; account state will be migrated between the underlying storage layers.

### Property

`secureStateAtRest` -

A flag indicating whether or not to use encrypted storage for the persisted account
state. If set to `true`, [SecureAbove22AccountStorage](#) will be used as a storage layer. As the name suggests,
account state will only by encrypted on Android API 23+. Otherwise, even if this flag is set to `true`, account state
will be stored in plaintext.



Default value of `false` configures the plaintext version of account storage to be used, [SharedPrefAccountStorage](#).



Switching of this flag's values is supported; account state will be migrated between the underlying storage layers.

