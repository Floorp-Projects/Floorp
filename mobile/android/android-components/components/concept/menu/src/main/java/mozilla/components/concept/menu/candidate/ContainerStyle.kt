/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.menu.candidate

/**
 * Describes styling for the menu option container.
 *
 * @property isVisible When false, the option will not be displayed.
 * @property isEnabled When false, the option will be greyed out and disabled.
 */
data class ContainerStyle(
    val isVisible: Boolean = true,
    val isEnabled: Boolean = true,
)
