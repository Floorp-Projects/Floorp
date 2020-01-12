[android-components](../index.md) / [mozilla.components.browser.engine.gecko.autofill](./index.md)

## Package mozilla.components.browser.engine.gecko.autofill

### Types

| Name | Summary |
|---|---|
| [GeckoLoginDelegateWrapper](-gecko-login-delegate-wrapper/index.md) | `class GeckoLoginDelegateWrapper : `[`Delegate`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/LoginStorage/Delegate.html)<br>This class exists only to convert incoming [LoginEntry](#) arguments into [Login](../mozilla.components.concept.storage/-login/index.md)s, then forward them to [storageDelegate](#). This allows us to avoid duplicating [LoginStorageDelegate](../mozilla.components.concept.storage/-login-storage-delegate/index.md) code between different versions of GeckoView, by duplicating this wrapper instead. |
