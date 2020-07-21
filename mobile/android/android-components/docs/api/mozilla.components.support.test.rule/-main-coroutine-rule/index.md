[android-components](../../index.md) / [mozilla.components.support.test.rule](../index.md) / [MainCoroutineRule](./index.md)

# MainCoroutineRule

`@ExperimentalCoroutinesApi class MainCoroutineRule : TestWatcher` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/test/src/main/java/mozilla/components/support/test/rule/MainCoroutineRule.kt#L28)

JUnit rule to change Dispatchers.Main in coroutines.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `MainCoroutineRule(testDispatcher: CoroutineDispatcher = createTestCoroutinesDispatcher())`<br>JUnit rule to change Dispatchers.Main in coroutines. |

### Properties

| Name | Summary |
|---|---|
| [testDispatcher](test-dispatcher.md) | `val testDispatcher: CoroutineDispatcher` |

### Functions

| Name | Summary |
|---|---|
| [finished](finished.md) | `fun finished(description: Description?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [starting](starting.md) | `fun starting(description: Description?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
