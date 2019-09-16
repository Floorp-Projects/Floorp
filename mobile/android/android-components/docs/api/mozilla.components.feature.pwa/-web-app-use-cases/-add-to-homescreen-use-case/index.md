[android-components](../../../index.md) / [mozilla.components.feature.pwa](../../index.md) / [WebAppUseCases](../index.md) / [AddToHomescreenUseCase](./index.md)

# AddToHomescreenUseCase

`class AddToHomescreenUseCase` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/WebAppUseCases.kt#L52)

Let the user add the selected session to the homescreen.

If the selected session represents a Progressive Web App, then the
manifest will be saved and the web app will be launched based on the
manifest values.

Otherwise, the pinned shortcut will act like a simple bookmark for the site.

### Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `operator suspend fun invoke(overrideBasicShortcutName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
