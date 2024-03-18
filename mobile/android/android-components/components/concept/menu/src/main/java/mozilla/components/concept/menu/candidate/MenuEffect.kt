/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.menu.candidate

import androidx.annotation.ColorInt

/**
 * Describes an effect for the menu.
 * Effects can also alter the button to open the menu.
 */
sealed class MenuEffect

/**
 * Describes an effect for a menu candidate and its container.
 * Effects can also alter the button that opens the menu.
 */
sealed class MenuCandidateEffect : MenuEffect()

/**
 * Describes an effect for a menu icon.
 * Effects can also alter the button that opens the menu.
 */
sealed class MenuIconEffect : MenuEffect()

/**
 * Displays a notification dot.
 * Used for highlighting new features to the user, such as what's new or a recommended feature.
 *
 * @property notificationTint Tint for the notification dot displayed on the icon and menu button.
 */
data class LowPriorityHighlightEffect(
    @ColorInt val notificationTint: Int,
) : MenuIconEffect()

/**
 * Changes the background of the menu item.
 * Used for errors that require user attention, like sync errors.
 *
 * @property backgroundTint Tint for the menu item background color.
 * Also used to highlight the menu button.
 */
data class HighPriorityHighlightEffect(
    @ColorInt val backgroundTint: Int,
) : MenuCandidateEffect()
