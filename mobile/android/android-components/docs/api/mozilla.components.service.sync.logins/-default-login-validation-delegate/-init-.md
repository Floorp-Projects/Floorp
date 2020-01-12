[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [DefaultLoginValidationDelegate](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`DefaultLoginValidationDelegate(storage: `[`AsyncLoginsStorage`](../-async-logins-storage/index.md)`, keyStore: `[`SecureAbove22Preferences`](../../mozilla.components.lib.dataprotect/-secure-above22-preferences/index.md)`, scope: CoroutineScope = CoroutineScope(IO))`

A delegate that will check against [storage](#) to see if a given Login can be persisted, and return
information about why it can or cannot.

