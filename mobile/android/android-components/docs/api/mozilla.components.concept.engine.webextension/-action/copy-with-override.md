[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [Action](index.md) / [copyWithOverride](./copy-with-override.md)

# copyWithOverride

`fun copyWithOverride(override: `[`Action`](index.md)`): `[`Action`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/Action.kt#L37)

Returns a copy of this [Action](index.md) with the provided override applied e.g. for tab-specific overrides.

### Parameters

`override` - the action to use for overriding properties. Note that only the provided
(non-null) properties of the override will be applied, all other properties will remain
unchanged. An extension can send a tab-specific action and only include the properties
it wants to override for the tab.