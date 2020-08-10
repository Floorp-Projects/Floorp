[android-components](../../index.md) / [mozilla.components.support.base.memory](../index.md) / [MemoryConsumer](index.md) / [onTrimMemory](./on-trim-memory.md)

# onTrimMemory

`abstract fun onTrimMemory(level: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/memory/MemoryConsumer.kt#L25)

Notifies this component that it should try to release memory.

Should be called from a [ComponentCallbacks2](#) providing the level passed to
[ComponentCallbacks2.onTrimMemory](#).

### Parameters

`level` - The context of the trim, giving a hint of the amount of
trimming the application may like to perform. See constants in [ComponentCallbacks2](#).