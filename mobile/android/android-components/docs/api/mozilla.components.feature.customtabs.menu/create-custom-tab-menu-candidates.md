[android-components](../index.md) / [mozilla.components.feature.customtabs.menu](index.md) / [createCustomTabMenuCandidates](./create-custom-tab-menu-candidates.md)

# createCustomTabMenuCandidates

`fun `[`CustomTabSessionState`](../mozilla.components.browser.state.state/-custom-tab-session-state/index.md)`.createCustomTabMenuCandidates(context: <ERROR CLASS>): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TextMenuCandidate`](../mozilla.components.concept.menu.candidate/-text-menu-candidate/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/customtabs/src/main/java/mozilla/components/feature/customtabs/menu/CustomTabMenuCandidates.kt#L19)

Build menu items displayed when the 3-dot overflow menu is opened.
These items are provided by the app that creates the custom tab,
and should be inserted alongside menu items created by the browser.

