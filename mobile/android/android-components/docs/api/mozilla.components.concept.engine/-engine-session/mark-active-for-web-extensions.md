[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [EngineSession](index.md) / [markActiveForWebExtensions](./mark-active-for-web-extensions.md)

# markActiveForWebExtensions

`open fun markActiveForWebExtensions(active: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSession.kt#L584)

Marks this session active/inactive for web extensions to support
tabs.query({active: true}).

### Parameters

`active` - whether this session should be marked as active or inactive.