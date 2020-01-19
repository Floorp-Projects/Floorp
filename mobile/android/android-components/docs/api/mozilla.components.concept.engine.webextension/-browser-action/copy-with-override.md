[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [BrowserAction](index.md) / [copyWithOverride](./copy-with-override.md)

# copyWithOverride

`fun copyWithOverride(override: `[`BrowserAction`](index.md)`): `[`BrowserAction`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/BrowserAction.kt#L35)

Returns a copy of this [BrowserAction](index.md) with the provided overrides applied e.g. for tab-specific overrides.

### Parameters

`override` - the action to use for overriding properties. Note that only the provided
(non-null) properties of the override will be applied, all other properties will remain unchanged.