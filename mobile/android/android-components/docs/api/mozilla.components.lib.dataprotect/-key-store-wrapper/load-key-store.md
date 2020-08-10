[android-components](../../index.md) / [mozilla.components.lib.dataprotect](../index.md) / [KeyStoreWrapper](index.md) / [loadKeyStore](./load-key-store.md)

# loadKeyStore

`open fun loadKeyStore(): `[`KeyStore`](http://docs.oracle.com/javase/7/docs/api/java/security/KeyStore.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/dataprotect/src/main/java/mozilla/components/lib/dataprotect/Keystore.kt#L121)

Creates and initializes the KeyStore in use.

This method loads a`"AndroidKeyStore"` type KeyStore.

Subclasses override this to load a KeyStore appropriate to the testing environment.

### Exceptions

`KeyStoreException` - if the type of store is not supported

**Return**
The KeyStore, already initialized

