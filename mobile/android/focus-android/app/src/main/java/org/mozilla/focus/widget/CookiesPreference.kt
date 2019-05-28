/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.widget

import android.content.Context
import androidx.preference.ListPreference
import androidx.preference.PreferenceViewHolder
import android.util.AttributeSet
import org.mozilla.focus.utils.Settings

/**
 * Autocomplete preference that will show a sub screen to configure the autocomplete behavior.
 */
class CookiesPreference(context: Context?, attrs: AttributeSet?) : ListPreference(context, attrs) {

    override fun onBindViewHolder(holder: PreferenceViewHolder?) {
        super.onBindViewHolder(holder)
        updateSummary()
    }

    override fun notifyChanged() {
        super.notifyChanged()
        updateSummary()
    }

    fun updateSummary() {
        val settings = Settings.getInstance(context)
        super.setSummary(settings.shouldBlockCookiesValue())
    }
}
