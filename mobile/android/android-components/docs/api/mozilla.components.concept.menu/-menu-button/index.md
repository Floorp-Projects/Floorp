[android-components](../../index.md) / [mozilla.components.concept.menu](../index.md) / [MenuButton](./index.md)

# MenuButton

`interface MenuButton : `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/menu/src/main/java/mozilla/components/concept/menu/MenuButton.kt#L16)

A `three-dot` button used for expanding menus.

If you are using a browser toolbar, do not use this class directly.

### Types

| Name | Summary |
|---|---|
| [Observer](-observer/index.md) | `interface Observer`<br>Observer for the menu button. |

### Properties

| Name | Summary |
|---|---|
| [menuController](menu-controller.md) | `abstract var menuController: `[`MenuController`](../-menu-controller/index.md)`?`<br>Sets a [MenuController](../-menu-controller/index.md) that will be used to create a menu when this button is clicked. |

### Functions

| Name | Summary |
|---|---|
| [setColorFilter](set-color-filter.md) | `abstract fun setColorFilter(color: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the tint of the 3-dot menu icon. |
| [setEffect](set-effect.md) | `abstract fun setEffect(effect: `[`MenuEffect`](../../mozilla.components.concept.menu.candidate/-menu-effect.md)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Show the indicator for a browser menu effect. |

### Inherited Functions

| Name | Summary |
|---|---|
| [isObserved](../../mozilla.components.support.base.observer/-observable/is-observed.md) | `abstract fun isObserved(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>If the observable has registered observers. |
| [notifyAtLeastOneObserver](../../mozilla.components.support.base.observer/-observable/notify-at-least-one-observer.md) | `abstract fun notifyAtLeastOneObserver(block: `[`T`](../../mozilla.components.support.base.observer/-observable/index.md#T)`.() -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies all registered observers about a change. If there is no observer the notification is queued and sent to the first observer that is registered. |
| [notifyObservers](../../mozilla.components.support.base.observer/-observable/notify-observers.md) | `abstract fun notifyObservers(block: `[`T`](../../mozilla.components.support.base.observer/-observable/index.md#T)`.() -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies all registered observers about a change. |
| [pauseObserver](../../mozilla.components.support.base.observer/-observable/pause-observer.md) | `abstract fun pauseObserver(observer: `[`T`](../../mozilla.components.support.base.observer/-observable/index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Pauses the provided observer. No notifications will be sent to this observer until [resumeObserver](../../mozilla.components.support.base.observer/-observable/resume-observer.md) is called. |
| [register](../../mozilla.components.support.base.observer/-observable/register.md) | `abstract fun register(observer: `[`T`](../../mozilla.components.support.base.observer/-observable/index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>`abstract fun register(observer: `[`T`](../../mozilla.components.support.base.observer/-observable/index.md#T)`, owner: LifecycleOwner, autoPause: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>`abstract fun register(observer: `[`T`](../../mozilla.components.support.base.observer/-observable/index.md#T)`, view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers an observer to get notified about changes. |
| [resumeObserver](../../mozilla.components.support.base.observer/-observable/resume-observer.md) | `abstract fun resumeObserver(observer: `[`T`](../../mozilla.components.support.base.observer/-observable/index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Resumes the provided observer. Notifications sent since it was last paused (see [pauseObserver](../../mozilla.components.support.base.observer/-observable/pause-observer.md)]) are lost and will not be re-delivered. |
| [unregister](../../mozilla.components.support.base.observer/-observable/unregister.md) | `abstract fun unregister(observer: `[`T`](../../mozilla.components.support.base.observer/-observable/index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Unregisters an observer. |
| [unregisterObservers](../../mozilla.components.support.base.observer/-observable/unregister-observers.md) | `abstract fun unregisterObservers(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Unregisters all observers. |
| [wrapConsumers](../../mozilla.components.support.base.observer/-observable/wrap-consumers.md) | `abstract fun <R> wrapConsumers(block: `[`T`](../../mozilla.components.support.base.observer/-observable/index.md#T)`.(`[`R`](../../mozilla.components.support.base.observer/-observable/wrap-consumers.md#R)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<(`[`R`](../../mozilla.components.support.base.observer/-observable/wrap-consumers.md#R)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>Returns a list of lambdas wrapping a consuming method of an observer. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [MenuButton](../../mozilla.components.browser.menu.view/-menu-button/index.md) | `class MenuButton : `[`MenuButton`](./index.md)`, `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-observer/index.md)`>`<br>A `three-dot` button used for expanding menus. |
| [MenuButton2](../../mozilla.components.browser.menu2.view/-menu-button2/index.md) | `class MenuButton2 : `[`MenuButton`](./index.md)`, `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-observer/index.md)`>`<br>A `three-dot` button used for expanding menus. |
