[android-components](../../index.md) / [mozilla.components.feature.share](../index.md) / [RecentApp](./index.md)

# RecentApp

`interface RecentApp` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/share/src/main/java/mozilla/components/feature/share/RecentApp.kt#L13)

Interface used for adapting recent apps database entities

### Properties

| Name | Summary |
|---|---|
| [activityName](activity-name.md) | `abstract val activityName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The activityName of the recent app. |
| [score](score.md) | `abstract val score: `[`Double`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-double/index.html)<br>The score of the recent app (calculated based on number of selections - decay) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
