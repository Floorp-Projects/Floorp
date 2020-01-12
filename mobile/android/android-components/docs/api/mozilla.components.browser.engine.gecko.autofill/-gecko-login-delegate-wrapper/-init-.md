[android-components](../../index.md) / [mozilla.components.browser.engine.gecko.autofill](../index.md) / [GeckoLoginDelegateWrapper](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`GeckoLoginDelegateWrapper(storageDelegate: `[`LoginStorageDelegate`](../../mozilla.components.concept.storage/-login-storage-delegate/index.md)`)`

This class exists only to convert incoming [LoginEntry](#) arguments into [Login](../../mozilla.components.concept.storage/-login/index.md)s, then forward
them to [storageDelegate](#). This allows us to avoid duplicating [LoginStorageDelegate](../../mozilla.components.concept.storage/-login-storage-delegate/index.md) code
between different versions of GeckoView, by duplicating this wrapper instead.

