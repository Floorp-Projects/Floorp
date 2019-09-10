/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import android.util.AttributeSet
import android.view.View
import android.widget.RadioButton
import android.widget.TextView
import androidx.core.content.res.TypedArrayUtils.getAttr
import androidx.core.content.withStyledAttributes
import androidx.core.text.HtmlCompat
import androidx.preference.Preference
import androidx.preference.PreferenceViewHolder
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.settings

class RadioButtonPreference @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : Preference(context, attrs) {
    private val radioGroups = mutableListOf<RadioButtonPreference>()
    private lateinit var summaryView: TextView
    private lateinit var radioButton: RadioButton
    private var shouldSummaryBeParsedAsHtmlContent: Boolean = true
    private var defaultValue: Boolean = false
    private var clickListener: (() -> Unit)? = null

    init {
        layoutResource = R.layout.preference_widget_radiobutton

        context.withStyledAttributes(
            attrs,
            androidx.preference.R.styleable.Preference,
            getAttr(context, androidx.preference.R.attr.preferenceStyle, android.R.attr.preferenceStyle),
            0
        ) {
            defaultValue = when {
                hasValue(androidx.preference.R.styleable.Preference_defaultValue) ->
                    getBoolean(androidx.preference.R.styleable.Preference_defaultValue, false)
                hasValue(androidx.preference.R.styleable.Preference_android_defaultValue) ->
                    getBoolean(androidx.preference.R.styleable.Preference_android_defaultValue, false)
                else -> false
            }
        }
    }

    fun addToRadioGroup(radioPreference: RadioButtonPreference) {
        radioGroups.add(radioPreference)
    }

    fun onClickListener(listener: (() -> Unit)) {
        clickListener = listener
    }

    override fun onBindViewHolder(holder: PreferenceViewHolder) {
        super.onBindViewHolder(holder)

        bindRadioButton(holder)

        bindTitle(holder)

        bindSummaryView(holder)

        setOnPreferenceClickListener {
            updateRadioValue(true)

            toggleRadioGroups()
            clickListener?.invoke()
            true
        }
    }

    private fun updateRadioValue(isChecked: Boolean) {
        persistBoolean(isChecked)
        radioButton.isChecked = isChecked
        context.settings.preferences.edit().putBoolean(key, isChecked)
            .apply()
    }

    private fun bindRadioButton(holder: PreferenceViewHolder) {
        radioButton = holder.findViewById(R.id.radio_button) as RadioButton
        radioButton.isChecked = context.settings.preferences.getBoolean(key, false)
        radioButton.setStartCheckedIndicator()
    }

    private fun toggleRadioGroups() {
        if (radioButton.isChecked) {
            radioGroups.forEach { it.updateRadioValue(false) }
        }
    }

    private fun bindTitle(holder: PreferenceViewHolder) {
        val titleView = holder.findViewById(R.id.title) as TextView

        if (!title.isNullOrEmpty()) {
            titleView.text = title
        }
    }

    private fun bindSummaryView(holder: PreferenceViewHolder) {
        summaryView = holder.findViewById(R.id.widget_summary) as TextView

        if (!summary.isNullOrEmpty()) {
            summaryView.text = if (shouldSummaryBeParsedAsHtmlContent) {
                HtmlCompat.fromHtml(summary.toString(), HtmlCompat.FROM_HTML_MODE_COMPACT)
            } else {
                summary
            }

            summaryView.visibility = View.VISIBLE
        } else {
            summaryView.visibility = View.GONE
        }
    }
}
