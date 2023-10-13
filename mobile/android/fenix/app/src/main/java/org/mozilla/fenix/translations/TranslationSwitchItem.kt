/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations

/**
 * TranslationSwitchItem that will appear on Translation screens.
 *
 * @property textLabel The text that will appear on the switch item.
 * @property isChecked Whether the switch is checked or not.
 * @property hasDivider Whether a divider should appear under the switch item.
 * @property onStateChange Invoked when the switch item is clicked,
 * the new checked state is passed into the callback.
 */
data class TranslationSwitchItem(
    val textLabel: String,
    val isChecked: Boolean,
    val hasDivider: Boolean,
    val onStateChange: (Boolean) -> Unit,
)
