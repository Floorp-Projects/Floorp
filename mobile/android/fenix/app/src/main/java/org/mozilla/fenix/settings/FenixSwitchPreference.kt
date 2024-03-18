/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import android.util.AttributeSet
import androidx.preference.CheckBoxPreference
import androidx.preference.SwitchPreference
import org.mozilla.fenix.R

/**
 * Switch Preference that adheres to Fenix styling.
 *
 * **Note:** The [SwitchPreference] layout internal id "switch_widget" has a min API of 24, so we use
 * [CheckBoxPreference] instead and use the layout internal id "checkbox".
 */
class FenixSwitchPreference @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
) : CheckBoxPreference(context, attrs) {
    init {
        layoutResource = R.layout.preference_widget_switch_fenix_style
    }
}
