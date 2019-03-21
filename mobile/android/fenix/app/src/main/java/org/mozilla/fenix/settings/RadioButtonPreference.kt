/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.view.View
import androidx.preference.PreferenceViewHolder
import android.widget.TextView
import android.content.Context
import android.text.TextUtils
import android.util.AttributeSet
import android.widget.RadioButton
import androidx.preference.Preference
import org.mozilla.fenix.R

class RadioButtonPreference : Preference {
    private val radioGroups = mutableListOf<RadioButtonPreference>()
    private lateinit var summaryView: TextView
    private lateinit var radioButton: RadioButton

    init {
        layoutResource = R.layout.preference_widget_radiobutton
    }

    constructor(context: Context, attrs: AttributeSet) : super(context, attrs)

    constructor(context: Context, attrs: AttributeSet, defStyleAttr: Int) : super(
        context,
        attrs,
        defStyleAttr
    )

    fun addToRadioGroup(radioPreference: RadioButtonPreference) {
        radioGroups.add(radioPreference)
    }

    override fun onBindViewHolder(holder: PreferenceViewHolder) {
        super.onBindViewHolder(holder)

        bindRadioButton(holder)

        bindTitle(holder)

        bindSummaryView(holder)

        setOnPreferenceClickListener {
            updateRadioValue(true)

            toggleRadioGroups()
            true
        }
    }

    private fun updateRadioValue(isChecked: Boolean) {
        persistBoolean(isChecked)
        radioButton.isChecked = isChecked
    }

    private fun bindRadioButton(holder: PreferenceViewHolder) {
        radioButton = holder.findViewById(R.id.radio_button) as RadioButton
        radioButton.isChecked = getPersistedBoolean(false)
    }

    private fun toggleRadioGroups() {
        if (radioButton.isChecked) {
            radioGroups.forEach { it.updateRadioValue(false) }
        }
    }

    private fun bindTitle(holder: PreferenceViewHolder) {
        val titleView = holder.findViewById(R.id.title) as TextView

        if (!TextUtils.isEmpty(title)) {
            titleView.text = title
        }
    }

    private fun bindSummaryView(holder: PreferenceViewHolder) {
        summaryView = holder.findViewById(R.id.widget_summary) as TextView
        if (!TextUtils.isEmpty(summary)) {
            summaryView.text = summary
            summaryView.visibility = View.VISIBLE
        } else {
            summaryView.visibility = View.GONE
        }
    }
}
