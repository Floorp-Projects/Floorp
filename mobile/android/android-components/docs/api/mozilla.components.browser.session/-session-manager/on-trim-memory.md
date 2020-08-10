[android-components](../../index.md) / [mozilla.components.browser.session](../index.md) / [SessionManager](index.md) / [onTrimMemory](./on-trim-memory.md)

# onTrimMemory

`@Synchronized fun onTrimMemory(level: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/SessionManager.kt#L404)

Overrides [MemoryConsumer.onTrimMemory](../../mozilla.components.support.base.memory/-memory-consumer/on-trim-memory.md)

Notifies this component that it should try to release memory.

Should be called from a [ComponentCallbacks2](#) providing the level passed to
[ComponentCallbacks2.onTrimMemory](#).

### Parameters

`level` - The context of the trim, giving a hint of the amount of
trimming the application may like to perform. See constants in [ComponentCallbacks2](#).