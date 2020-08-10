[android-components](../../index.md) / [mozilla.components.feature.share](../index.md) / [RecentAppsStorage](./index.md)

# RecentAppsStorage

`class RecentAppsStorage` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/share/src/main/java/mozilla/components/feature/share/RecentAppsStorage.kt#L16)

Class used for storing and retrieving the most recent apps

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `RecentAppsStorage(context: <ERROR CLASS>)`<br>Class used for storing and retrieving the most recent apps |

### Functions

| Name | Summary |
|---|---|
| [deleteRecentApp](delete-recent-app.md) | `fun deleteRecentApp(activityName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Deletes an app form the recent apps list |
| [getRecentAppsUpTo](get-recent-apps-up-to.md) | `fun getRecentAppsUpTo(limit: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`RecentApp`](../-recent-app/index.md)`>`<br>Get a descending ordered list of the most recent apps |
| [updateDatabaseWithNewApps](update-database-with-new-apps.md) | `fun updateDatabaseWithNewApps(activityNames: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>If there are apps that could resolve our share and are not added in our database, we add them with a 0 count, so they can be updated later when a user uses that app |
| [updateRecentApp](update-recent-app.md) | `fun updateRecentApp(selectedActivityName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Increment the value stored in the database for the selected app. Then, apply a decay to all other apps in the database. This allows newly installed apps to catch up and appear in the most recent section faster. We do not need to handle overflow as it's not reasonably expected to reach Double.MAX_VALUE for users |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
