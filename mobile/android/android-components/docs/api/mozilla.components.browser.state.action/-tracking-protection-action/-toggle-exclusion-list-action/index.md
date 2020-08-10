[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [TrackingProtectionAction](../index.md) / [ToggleExclusionListAction](./index.md)

# ToggleExclusionListAction

`data class ToggleExclusionListAction : `[`TrackingProtectionAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L324)

Updates the [TrackingProtectionState.ignoredOnTrackingProtection](../../../mozilla.components.browser.state.state/-tracking-protection-state/ignored-on-tracking-protection.md) flag.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ToggleExclusionListAction(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, excluded: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`)`<br>Updates the [TrackingProtectionState.ignoredOnTrackingProtection](../../../mozilla.components.browser.state.state/-tracking-protection-state/ignored-on-tracking-protection.md) flag. |

### Properties

| Name | Summary |
|---|---|
| [excluded](excluded.md) | `val excluded: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [tabId](tab-id.md) | `val tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
