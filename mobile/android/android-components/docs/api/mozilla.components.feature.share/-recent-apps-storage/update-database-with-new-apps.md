[android-components](../../index.md) / [mozilla.components.feature.share](../index.md) / [RecentAppsStorage](index.md) / [updateDatabaseWithNewApps](./update-database-with-new-apps.md)

# updateDatabaseWithNewApps

`fun updateDatabaseWithNewApps(activityNames: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/share/src/main/java/mozilla/components/feature/share/RecentAppsStorage.kt#L53)

If there are apps that could resolve our share and are not added in our database, we add them
with a 0 count, so they can be updated later when a user uses that app

