/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.preference

import android.content.Context
import android.util.AttributeSet
import androidx.appcompat.widget.SwitchCompat
import androidx.preference.Preference
import androidx.preference.PreferenceViewHolder
import mozilla.components.feature.autofill.AutofillUseCases
import mozilla.components.feature.autofill.R

/**
 * Preference showing a switch to enable this app as the preferred autofill service of the user.
 *
 * When getting enabled this preference will launch Android's system setting for selecting an
 * autofill service.
 */
class AutofillPreference(
    context: Context,
    attrs: AttributeSet? = null,
) : Preference(context, attrs) {
    private val useCases = AutofillUseCases()
    private var switchView: SwitchCompat? = null

    init {
        widgetLayoutResource = R.layout.mozac_feature_autofill_preference
        isVisible = useCases.isSupported(context)
    }

    override fun onBindViewHolder(holder: PreferenceViewHolder) {
        super.onBindViewHolder(holder)

        switchView = holder.findViewById(R.id.switch_widget) as SwitchCompat

        update()
    }

    override fun onClick() {
        super.onClick()

        if (switchView?.isChecked == true) {
            useCases.disable(context)
            switchView?.isChecked = false
        } else {
            useCases.enable(context)
        }
    }

    /**
     * Updates the preference (on/off) based on whether this app is set as the user's autofill
     * service.
     */
    fun update() {
        switchView?.isChecked = useCases.isEnabled(context)
    }
}
