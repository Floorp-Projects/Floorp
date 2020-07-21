[android-components](../index.md) / [mozilla.components.concept.menu.candidate](./index.md)

## Package mozilla.components.concept.menu.candidate

### Types

| Name | Summary |
|---|---|
| [CompoundMenuCandidate](-compound-menu-candidate/index.md) | `data class CompoundMenuCandidate : `[`MenuCandidate`](-menu-candidate/index.md)<br>Menu option that shows a switch or checkbox. |
| [ContainerStyle](-container-style/index.md) | `data class ContainerStyle`<br>Describes styling for the menu option container. |
| [DecorativeTextMenuCandidate](-decorative-text-menu-candidate/index.md) | `data class DecorativeTextMenuCandidate : `[`MenuCandidate`](-menu-candidate/index.md)<br>Menu option that displays static text. |
| [DividerMenuCandidate](-divider-menu-candidate/index.md) | `data class DividerMenuCandidate : `[`MenuCandidate`](-menu-candidate/index.md)<br>Menu option to display a horizontal divider. |
| [DrawableButtonMenuIcon](-drawable-button-menu-icon/index.md) | `data class DrawableButtonMenuIcon : `[`MenuIcon`](-menu-icon.md)`, `[`MenuIconWithDrawable`](-menu-icon-with-drawable/index.md)<br>Menu icon that displays an image button. |
| [DrawableMenuIcon](-drawable-menu-icon/index.md) | `data class DrawableMenuIcon : `[`MenuIcon`](-menu-icon.md)`, `[`MenuIconWithDrawable`](-menu-icon-with-drawable/index.md)<br>Menu icon that displays a drawable. |
| [HighPriorityHighlightEffect](-high-priority-highlight-effect/index.md) | `data class HighPriorityHighlightEffect : `[`MenuCandidateEffect`](-menu-candidate-effect.md)<br>Changes the background of the menu item. Used for errors that require user attention, like sync errors. |
| [LowPriorityHighlightEffect](-low-priority-highlight-effect/index.md) | `data class LowPriorityHighlightEffect : `[`MenuIconEffect`](-menu-icon-effect.md)<br>Displays a notification dot. Used for highlighting new features to the user, such as what's new or a recommended feature. |
| [MenuCandidate](-menu-candidate/index.md) | `sealed class MenuCandidate`<br>Menu option data classes to be shown in the browser menu. |
| [MenuCandidateEffect](-menu-candidate-effect.md) | `sealed class MenuCandidateEffect : `[`MenuEffect`](-menu-effect.md)<br>Describes an effect for a menu candidate and its container. Effects can also alter the button that opens the menu. |
| [MenuEffect](-menu-effect.md) | `sealed class MenuEffect`<br>Describes an effect for the menu. Effects can also alter the button to open the menu. |
| [MenuIcon](-menu-icon.md) | `sealed class MenuIcon`<br>Menu option data classes to be shown alongside menu options |
| [MenuIconEffect](-menu-icon-effect.md) | `sealed class MenuIconEffect : `[`MenuEffect`](-menu-effect.md)<br>Describes an effect for a menu icon. Effects can also alter the button that opens the menu. |
| [MenuIconWithDrawable](-menu-icon-with-drawable/index.md) | `interface MenuIconWithDrawable`<br>Interface shared by all [MenuIcon](-menu-icon.md)s with drawables. |
| [NestedMenuCandidate](-nested-menu-candidate/index.md) | `data class NestedMenuCandidate : `[`MenuCandidate`](-menu-candidate/index.md)<br>Menu option that opens a nested sub menu. |
| [RowMenuCandidate](-row-menu-candidate/index.md) | `data class RowMenuCandidate : `[`MenuCandidate`](-menu-candidate/index.md)<br>Displays a row of small menu options. |
| [SmallMenuCandidate](-small-menu-candidate/index.md) | `data class SmallMenuCandidate`<br>Small icon button menu option. Can only be used with [RowMenuCandidate](-row-menu-candidate/index.md). |
| [TextMenuCandidate](-text-menu-candidate/index.md) | `data class TextMenuCandidate : `[`MenuCandidate`](-menu-candidate/index.md)<br>Interactive menu option that displays some text. |
| [TextMenuIcon](-text-menu-icon/index.md) | `data class TextMenuIcon : `[`MenuIcon`](-menu-icon.md)<br>Menu icon to display additional text at the end of a menu option. |
| [TextStyle](-text-style/index.md) | `data class TextStyle`<br>Describes styling for text inside a menu option. |

### Annotations

| Name | Summary |
|---|---|
| [TextAlignment](-text-alignment/index.md) | `annotation class TextAlignment`<br>Enum for text alignment values. |
| [TypefaceStyle](-typeface-style/index.md) | `annotation class TypefaceStyle`<br>Enum for [Typeface](#) values. |
