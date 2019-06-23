/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import android.text.TextUtils
import android.util.AttributeSet
import android.view.View
import android.widget.RadioButton
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.core.content.res.TypedArrayUtils
import androidx.core.content.withStyledAttributes
import androidx.core.text.HtmlCompat
import androidx.preference.Preference
import androidx.preference.PreferenceViewHolder
import org.mozilla.fenix.R
import org.mozilla.fenix.ThemeManager
import org.mozilla.fenix.utils.Settings

class RadioButtonPreference @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    attributeSetId: Int = 0
) : Preference(context, attrs, attributeSetId) {
    private val radioGroups = mutableListOf<RadioButtonPreference>()
    private lateinit var summaryView: TextView
    private lateinit var radioButton: RadioButton
    var shouldSummaryBeParsedAsHtmlContent: Boolean = true
    private var defaultValue: Boolean = false
    private var clickListener: (() -> Unit)? = null

    init {
        layoutResource = R.layout.preference_widget_radiobutton

        context.withStyledAttributes(
            attrs,
            androidx.preference.R.styleable.Preference,
            TypedArrayUtils.getAttr(
                context, androidx.preference.R.attr.preferenceStyle, android.R.attr.preferenceStyle
            ),
            0
        ) {
            if (hasValue(androidx.preference.R.styleable.Preference_defaultValue)) {
                defaultValue = getBoolean(
                    androidx.preference.R.styleable.Preference_defaultValue,
                    false
                )
            } else if (hasValue(androidx.preference.R.styleable.Preference_android_defaultValue)) {
                defaultValue = getBoolean(
                    androidx.preference.R.styleable.Preference_android_defaultValue,
                    false
                )
            }
        }
    }

    /* In devices with Android 6, when we use android:button="@null" android:drawableStart doesn't work via xml
     * as a result we have to apply it programmatically. More info about this issue https://github.com/mozilla-mobile/fenix/issues/1414
    */
    fun RadioButton.setStartCheckedIndicator() {
        val attr =
            ThemeManager.resolveAttribute(android.R.attr.listChoiceIndicatorSingle, context)
        val buttonDrawable = ContextCompat.getDrawable(context, attr)
        buttonDrawable.apply {
            this?.setBounds(0, 0, this.intrinsicWidth, this.intrinsicHeight)
        }
        this.setCompoundDrawables(buttonDrawable, null, null, null)
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
        Settings.getInstance(context).preferences.edit().putBoolean(key, isChecked)
            .apply()
    }

    private fun bindRadioButton(holder: PreferenceViewHolder) {
        radioButton = holder.findViewById(R.id.radio_button) as RadioButton
        radioButton.isChecked = Settings.getInstance(context).preferences.getBoolean(key, false)
        radioButton.setStartCheckedIndicator()
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
                summaryView.text =
                    HtmlCompat.fromHtml(summary.toString(), HtmlCompat.FROM_HTML_MODE_COMPACT)
            } else {
                summaryView.text = summary
            }
            summaryView.visibility = View.VISIBLE
        } else {
            summaryView.visibility = View.GONE
        }
    }
}
