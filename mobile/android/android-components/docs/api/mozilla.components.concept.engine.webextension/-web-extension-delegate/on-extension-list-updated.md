[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [WebExtensionDelegate](index.md) / [onExtensionListUpdated](./on-extension-list-updated.md)

# onExtensionListUpdated

`open fun onExtensionListUpdated(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtensionDelegate.kt#L134)

Invoked when the list of installed extensions has been updated in the engine
(the web extension runtime). This happens as a result of debugging tools (e.g
web-ext) installing temporary extensions. It does not happen in the regular flow
of installing / uninstalling extensions by the user.

