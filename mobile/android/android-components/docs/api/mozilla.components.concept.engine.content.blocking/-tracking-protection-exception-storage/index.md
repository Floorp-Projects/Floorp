[android-components](../../index.md) / [mozilla.components.concept.engine.content.blocking](../index.md) / [TrackingProtectionExceptionStorage](./index.md)

# TrackingProtectionExceptionStorage

`interface TrackingProtectionExceptionStorage` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/content/blocking/TrackingProtectionExceptionStorage.kt#L12)

A contract that define how a tracking protection storage must behave.

### Functions

| Name | Summary |
|---|---|
| [add](add.md) | `abstract fun add(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds a new [session](add.md#mozilla.components.concept.engine.content.blocking.TrackingProtectionExceptionStorage$add(mozilla.components.concept.engine.EngineSession)/session) to the exception list. |
| [contains](contains.md) | `abstract fun contains(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, onResult: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Indicates if a given [session](contains.md#mozilla.components.concept.engine.content.blocking.TrackingProtectionExceptionStorage$contains(mozilla.components.concept.engine.EngineSession, kotlin.Function1((kotlin.Boolean, kotlin.Unit)))/session) is in the exception list. |
| [fetchAll](fetch-all.md) | `abstract fun fetchAll(onResult: (`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TrackingProtectionException`](../-tracking-protection-exception/index.md)`>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Fetch all domains that will be ignored for tracking protection. |
| [remove](remove.md) | `abstract fun remove(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes a [session](remove.md#mozilla.components.concept.engine.content.blocking.TrackingProtectionExceptionStorage$remove(mozilla.components.concept.engine.EngineSession)/session) from the exception list.`abstract fun remove(exception: `[`TrackingProtectionException`](../-tracking-protection-exception/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes a [exception](remove.md#mozilla.components.concept.engine.content.blocking.TrackingProtectionExceptionStorage$remove(mozilla.components.concept.engine.content.blocking.TrackingProtectionException)/exception) from the exception list. |
| [removeAll](remove-all.md) | `abstract fun removeAll(activeSessions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`>? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes all domains from the exception list. |
| [restore](restore.md) | `abstract fun restore(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Restore all domains stored in the storage. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
