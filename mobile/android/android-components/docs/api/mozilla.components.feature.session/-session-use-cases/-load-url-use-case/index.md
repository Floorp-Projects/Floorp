[android-components](../../../index.md) / [mozilla.components.feature.session](../../index.md) / [SessionUseCases](../index.md) / [LoadUrlUseCase](./index.md)

# LoadUrlUseCase

`interface LoadUrlUseCase` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/SessionUseCases.kt#L30)

Contract for use cases that load a provided URL.

### Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `abstract fun invoke(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, flags: `[`LoadUrlFlags`](../../../mozilla.components.concept.engine/-engine-session/-load-url-flags/index.md)` = LoadUrlFlags.none()): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [AddNewPrivateTabUseCase](../../../mozilla.components.feature.tabs/-tabs-use-cases/-add-new-private-tab-use-case/index.md) | `class AddNewPrivateTabUseCase : `[`LoadUrlUseCase`](./index.md) |
| [AddNewTabUseCase](../../../mozilla.components.feature.tabs/-tabs-use-cases/-add-new-tab-use-case/index.md) | `class AddNewTabUseCase : `[`LoadUrlUseCase`](./index.md) |
| [DefaultLoadUrlUseCase](../-default-load-url-use-case/index.md) | `class DefaultLoadUrlUseCase : `[`LoadUrlUseCase`](./index.md) |
