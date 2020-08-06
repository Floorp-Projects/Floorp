[android-components](../../index.md) / [mozilla.components.feature.webcompat.reporter](../index.md) / [WebCompatReporterFeature](index.md) / [install](./install.md)

# install

`fun install(runtime: `[`WebExtensionRuntime`](../../mozilla.components.concept.engine.webextension/-web-extension-runtime/index.md)`, productName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "android-components"): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/webcompat-reporter/src/main/java/mozilla/components/feature/webcompat/reporter/WebCompatReporterFeature.kt#L41)

Installs the web extension in the runtime through the WebExtensionRuntime install method

### Parameters

`runtime` - a WebExtensionRuntime.

`productName` - a custom product name used to automatically label reports. Defaults to
"android-components".