[android-components](../../index.md) / [mozilla.components.lib.dataprotect](../index.md) / [SecureAbove22Preferences](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`SecureAbove22Preferences(context: <ERROR CLASS>, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, forceInsecure: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`

A wrapper around [SharedPreferences](#) which encrypts contents on supported API versions (23+).
Otherwise, this simply delegates to [SharedPreferences](#).

In rare circumstances (such as APK signing key rotation) a master key which protects this storage may be lost,
in which case previously stored values will be lost as well. Applications are encouraged to instrument such events.

### Parameters

`context` - A [Context](#), used for accessing [SharedPreferences](#).

`name` - A name for this storage, used for isolating different instances of [SecureAbove22Preferences](index.md).

`forceInsecure` - A flag indicating whether to force plaintext storage. If set to `true`,
[InsecurePreferencesImpl21](#) will be used as a storage layer, otherwise a storage implementation
will be decided based on Android API version, with a preference given to secure storage