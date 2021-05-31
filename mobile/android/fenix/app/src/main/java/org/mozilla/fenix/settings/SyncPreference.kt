/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import android.util.AttributeSet
import android.view.View
import androidx.appcompat.widget.SwitchCompat
import androidx.preference.PreferenceViewHolder
import androidx.preference.SwitchPreferenceCompat
import org.mozilla.fenix.R

/**
 * Variation of [SwitchPreferenceCompat] that uses a custom widgetLayoutResource in order to implement
 * visibility changes to it.
 * */
class SyncPreference @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : SwitchPreferenceCompat(context, attrs) {

    private var switchView: SwitchCompat? = null

    /**
     * Whether or not switch's toggle widget is visible.
     * */
    var isSwitchWidgetVisible: Boolean = false

    init {
        widgetLayoutResource = R.layout.preference_sync
    }

    /**
     * Updates the switch state.
     * */
    internal fun setSwitchCheckedState(isChecked: Boolean) {
        switchView?.isChecked = isChecked
    }

    override fun onBindViewHolder(holder: PreferenceViewHolder) {
        super.onBindViewHolder(holder)

        switchView = holder.findViewById(R.id.switch_widget) as SwitchCompat?

        switchView?.isChecked = isChecked
        switchView?.visibility = if (isSwitchWidgetVisible) View.VISIBLE else View.INVISIBLE
    }
}
