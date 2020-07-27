[android-components](../../../index.md) / [mozilla.components.concept.menu](../../index.md) / [MenuController](../index.md) / [Observer](./index.md)

# Observer

`interface Observer` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/menu/src/main/java/mozilla/components/concept/menu/MenuController.kt#L35)

Observer for the menu controller.

### Functions

| Name | Summary |
|---|---|
| [onDismiss](on-dismiss.md) | `open fun onDismiss(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called when the menu has been dismissed. |
| [onMenuListSubmit](on-menu-list-submit.md) | `open fun onMenuListSubmit(list: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`MenuCandidate`](../../../mozilla.components.concept.menu.candidate/-menu-candidate/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called when the menu contents have changed. |
