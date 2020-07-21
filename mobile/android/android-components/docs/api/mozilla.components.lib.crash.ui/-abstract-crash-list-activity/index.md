[android-components](../../index.md) / [mozilla.components.lib.crash.ui](../index.md) / [AbstractCrashListActivity](./index.md)

# AbstractCrashListActivity

`abstract class AbstractCrashListActivity : AppCompatActivity` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/ui/AbstractCrashListActivity.kt#L15)

Activity for displaying the list of reported crashes.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AbstractCrashListActivity()`<br>Activity for displaying the list of reported crashes. |

### Properties

| Name | Summary |
|---|---|
| [crashReporter](crash-reporter.md) | `abstract val crashReporter: `[`CrashReporter`](../../mozilla.components.lib.crash/-crash-reporter/index.md) |

### Functions

| Name | Summary |
|---|---|
| [onCrashServiceSelected](on-crash-service-selected.md) | `abstract fun onCrashServiceSelected(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Gets invoked whenever the user selects a crash reporting service. |
| [onCreate](on-create.md) | `open fun onCreate(savedInstanceState: <ERROR CLASS>?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
