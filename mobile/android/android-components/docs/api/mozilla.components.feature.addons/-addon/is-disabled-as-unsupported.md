[android-components](../../index.md) / [mozilla.components.feature.addons](../index.md) / [Addon](index.md) / [isDisabledAsUnsupported](./is-disabled-as-unsupported.md)

# isDisabledAsUnsupported

`fun isDisabledAsUnsupported(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/Addon.kt#L137)

Returns whether or not this [Addon](index.md) is currently disabled because it is not
supported. This is based on the installed extension state in the engine. An
addon can be disabled as unsupported and later become supported, so
both [isSupported](is-supported.md) and [isDisabledAsUnsupported](./is-disabled-as-unsupported.md) can be true.

