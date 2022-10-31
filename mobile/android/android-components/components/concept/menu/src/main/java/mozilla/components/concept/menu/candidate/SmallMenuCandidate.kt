/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.menu.candidate

/**
 * Small icon button menu option. Can only be used with [RowMenuCandidate].
 *
 * @property contentDescription Description of the icon.
 * @property icon Icon to display.
 * @property containerStyle Styling to apply to the container.
 * @property onLongClick Listener called when this menu option is long clicked.
 * @property onClick Click listener called when this menu option is clicked.
 */
data class SmallMenuCandidate(
    val contentDescription: String,
    val icon: DrawableMenuIcon,
    val containerStyle: ContainerStyle = ContainerStyle(),
    val onLongClick: (() -> Boolean)? = null,
    val onClick: () -> Unit = {},
)
