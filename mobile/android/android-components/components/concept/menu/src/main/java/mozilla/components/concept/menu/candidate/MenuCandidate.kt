/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.menu.candidate

/**
 * Menu option data classes to be shown in the browser menu.
 */
sealed class MenuCandidate {
    abstract val containerStyle: ContainerStyle
}

/**
 * Interactive menu option that displays some text.
 *
 * @property text Text to display.
 * @property start Icon to display before the text.
 * @property end Icon to display after the text.
 * @property textStyle Styling to apply to the text.
 * @property containerStyle Styling to apply to the container.
 * @property effect Effects to apply to the option.
 * @property onClick Click listener called when this menu option is clicked.
 */
data class TextMenuCandidate(
    val text: String,
    val start: MenuIcon? = null,
    val end: MenuIcon? = null,
    val textStyle: TextStyle = TextStyle(),
    override val containerStyle: ContainerStyle = ContainerStyle(),
    val effect: MenuCandidateEffect? = null,
    val onClick: () -> Unit = {},
) : MenuCandidate()

/**
 * Menu option that displays static text.
 *
 * @property text Text to display.
 * @property height Custom height for the menu option.
 * @property textStyle Styling to apply to the text.
 * @property containerStyle Styling to apply to the container.
 */
data class DecorativeTextMenuCandidate(
    val text: String,
    val height: Int? = null,
    val textStyle: TextStyle = TextStyle(),
    override val containerStyle: ContainerStyle = ContainerStyle(),
) : MenuCandidate()

/**
 * Menu option that shows a switch or checkbox.
 *
 * @property text Text to display.
 * @property start Icon to display before the text.
 * @property end Compound button to display after the text.
 * @property textStyle Styling to apply to the text.
 * @property containerStyle Styling to apply to the container.
 * @property effect Effects to apply to the option.
 * @property onCheckedChange Listener called when this menu option is checked or unchecked.
 */
data class CompoundMenuCandidate(
    val text: String,
    val isChecked: Boolean,
    val start: MenuIcon? = null,
    val end: ButtonType,
    val textStyle: TextStyle = TextStyle(),
    override val containerStyle: ContainerStyle = ContainerStyle(),
    val effect: MenuCandidateEffect? = null,
    val onCheckedChange: (Boolean) -> Unit = {},
) : MenuCandidate() {

    /**
     * Compound button types to display with the compound menu option.
     */
    enum class ButtonType {
        CHECKBOX,
        SWITCH,
    }
}

/**
 * Menu option that opens a nested sub menu.
 *
 * @property id Unique ID for this nested menu. Can be a resource ID.
 * @property text Text to display.
 * @property start Icon to display before the text.
 * @property end Icon to display after the text.
 * @property subMenuItems Nested menu items to display.
 * If null, this item will instead return to the root menu.
 * @property textStyle Styling to apply to the text.
 * @property containerStyle Styling to apply to the container.
 * @property effect Effects to apply to the option.
 */
data class NestedMenuCandidate(
    val id: Int,
    val text: String,
    val start: MenuIcon? = null,
    val end: DrawableMenuIcon? = null,
    val subMenuItems: List<MenuCandidate>? = emptyList(),
    val textStyle: TextStyle = TextStyle(),
    override val containerStyle: ContainerStyle = ContainerStyle(),
    val effect: MenuCandidateEffect? = null,
) : MenuCandidate()

/**
 * Displays a row of small menu options.
 *
 * @property items Small menu options to display.
 * @property containerStyle Styling to apply to the container.
 */
data class RowMenuCandidate(
    val items: List<SmallMenuCandidate>,
    override val containerStyle: ContainerStyle = ContainerStyle(),
) : MenuCandidate()

/**
 * Menu option to display a horizontal divider.
 *
 * @property containerStyle Styling to apply to the divider.
 */
data class DividerMenuCandidate(
    override val containerStyle: ContainerStyle = ContainerStyle(),
) : MenuCandidate()
