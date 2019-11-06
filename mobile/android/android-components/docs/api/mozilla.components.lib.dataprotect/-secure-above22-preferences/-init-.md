[android-components](../../index.md) / [mozilla.components.lib.dataprotect](../index.md) / [SecureAbove22Preferences](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`SecureAbove22Preferences(context: <ERROR CLASS>)`

A wrapper around [SharedPreferences](#) which encrypts contents on supported API versions (23+).
Otherwise, this simply delegates to [SharedPreferences](#).

In rare circumstances (such as APK signing key rotation) a master key which protects this storage may be lost,
in which case previously stored values will be lost as well. Applications are encouraged to instrument such events.

### Parameters

`context` - A [Context](#), used for accessing [SharedPreferences](#).