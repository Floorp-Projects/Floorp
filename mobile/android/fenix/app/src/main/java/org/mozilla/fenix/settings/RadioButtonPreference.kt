/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.view.View
import androidx.preference.PreferenceViewHolder
import android.widget.TextView
import android.content.Context
import android.content.res.TypedArray
import android.text.TextUtils
import android.util.AttributeSet
import android.widget.RadioButton
import androidx.core.content.res.TypedArrayUtils
import androidx.core.text.HtmlCompat
import androidx.preference.Preference
import org.mozilla.fenix.R

class RadioButtonPreference : Preference {
    private val radioGroups = mutableListOf<RadioButtonPreference>()
    private lateinit var summaryView: TextView
    private lateinit var radioButton: RadioButton
    var shouldSummaryBeParsedAsHtmlContent: Boolean = true
    private var defaultValue: Boolean = false
    private var clickListener: (() -> Unit)? = null

    init {
        layoutResource = R.layout.preference_widget_radiobutton
    }

    constructor(context: Context, attrs: AttributeSet) : super(context, attrs) {
        val typedArray = context.obtainStyledAttributes(
            attrs, androidx.preference.R.styleable.Preference, TypedArrayUtils.getAttr(
                context, androidx.preference.R.attr.preferenceStyle,
                android.R.attr.preferenceStyle
            ), 0
        )
        initDefaultValue(typedArray)
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
    }

    private fun bindRadioButton(holder: PreferenceViewHolder) {
        radioButton = holder.findViewById(R.id.radio_button) as RadioButton
        radioButton.isChecked = getPersistedBoolean(defaultValue)
    }

    private fun initDefaultValue(typedArray: TypedArray) {
        if (typedArray.hasValue(androidx.preference.R.styleable.Preference_defaultValue)) {
            defaultValue = typedArray.getBoolean(androidx.preference.R.styleable.Preference_defaultValue, false)
        } else if (typedArray.hasValue(androidx.preference.R.styleable.Preference_android_defaultValue)) {
            defaultValue = typedArray.getBoolean(androidx.preference.R.styleable.Preference_android_defaultValue, false)
        }
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
            if (shouldSummaryBeParsedAsHtmlContent) {
                summaryView.text = HtmlCompat.fromHtml(summary.toString(), HtmlCompat.FROM_HTML_MODE_COMPACT)
            } else {
                summaryView.text = summary
            }
            summaryView.visibility = View.VISIBLE
        } else {
            summaryView.visibility = View.GONE
        }
    }
}
