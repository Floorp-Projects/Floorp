[android-components](../../../index.md) / [mozilla.components.feature.tabs](../../index.md) / [TabsUseCases](../index.md) / [SelectTabUseCase](./index.md)

# SelectTabUseCase

`interface SelectTabUseCase` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tabs/src/main/java/mozilla/components/feature/tabs/TabsUseCases.kt#L26)

Contract for use cases that select a tab.

### Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `abstract operator fun invoke(session: `[`Session`](../../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Select given [session](invoke.md#mozilla.components.feature.tabs.TabsUseCases.SelectTabUseCase$invoke(mozilla.components.browser.session.Session)/session).`abstract operator fun invoke(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Select [Session](../../../mozilla.components.browser.session/-session/index.md) with the given [tabId](invoke.md#mozilla.components.feature.tabs.TabsUseCases.SelectTabUseCase$invoke(kotlin.String)/tabId). |

### Inheritors

| Name | Summary |
|---|---|
| [DefaultSelectTabUseCase](../-default-select-tab-use-case/index.md) | `class DefaultSelectTabUseCase : `[`SelectTabUseCase`](./index.md) |
