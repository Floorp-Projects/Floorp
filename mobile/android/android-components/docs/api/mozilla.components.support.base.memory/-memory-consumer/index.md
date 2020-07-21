[android-components](../../index.md) / [mozilla.components.support.base.memory](../index.md) / [MemoryConsumer](./index.md)

# MemoryConsumer

`interface MemoryConsumer` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/memory/MemoryConsumer.kt#L15)

Interface for components that can seize large amounts of memory and support trimming in low
memory situations.

Also see [ComponentCallbacks2](#).

### Functions

| Name | Summary |
|---|---|
| [onTrimMemory](on-trim-memory.md) | `abstract fun onTrimMemory(level: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies this component that it should try to release memory. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [BrowserIcons](../../mozilla.components.browser.icons/-browser-icons/index.md) | `class BrowserIcons : `[`MemoryConsumer`](./index.md)<br>Entry point for loading icons for websites. |
| [SessionManager](../../mozilla.components.browser.session/-session-manager/index.md) | `class SessionManager : `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](../../mozilla.components.browser.session/-session-manager/-observer/index.md)`>, `[`MemoryConsumer`](./index.md)<br>This class provides access to a centralized registry of all active sessions. |
