/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.annotation.SuppressLint
import android.content.Context
import android.content.SharedPreferences
import android.util.AttributeSet
import android.widget.RadioButton
import android.widget.TextView
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.content.res.TypedArrayUtils.getAttr
import androidx.core.content.withStyledAttributes
import androidx.core.text.HtmlCompat
import androidx.core.view.isVisible
import androidx.preference.Preference
import androidx.preference.PreferenceManager
import androidx.preference.PreferenceViewHolder
import mozilla.components.support.ktx.android.content.res.resolveAttribute
import mozilla.components.support.ktx.android.view.putCompoundDrawablesRelative
import org.mozilla.focus.R

interface GroupableRadioButton {
    fun updateRadioValue(isChecked: Boolean)

    fun addToRadioGroup(radioButton: GroupableRadioButton)
}

/**
 * Connect all the given radio buttons into a group,
 * so that when one radio is checked the others are unchecked.
 */
fun addToRadioGroup(vararg radios: GroupableRadioButton) {
    for (i in 0..radios.lastIndex) {
        for (j in (i + 1)..radios.lastIndex) {
            radios[i].addToRadioGroup(radios[j])
            radios[j].addToRadioGroup(radios[i])
        }
    }
}

fun Iterable<GroupableRadioButton>.uncheckAll() {
    forEach { it.updateRadioValue(isChecked = false) }
}

@SuppressLint("RestrictedApi")
open class RadioButtonPreference @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
) : Preference(context, attrs), GroupableRadioButton {
    private val radioGroups = mutableListOf<GroupableRadioButton>()
    private var summaryView: TextView? = null
    private var titleView: TextView? = null
    private var radioButton: RadioButton? = null
    private var shouldSummaryBeParsedAsHtmlContent: Boolean = true
    private var defaultValue: Boolean = false
    private var clickListener: (() -> Unit)? = null

    private val preferences: SharedPreferences = PreferenceManager.getDefaultSharedPreferences(context)

    val isChecked: Boolean
        get() = radioButton?.isChecked == true

    init {
        layoutResource = R.layout.preference_radio_button

        context.withStyledAttributes(
            attrs,
            R.styleable.RadioButtonPreference,
            getAttr(
                context,
                androidx.preference.R.attr.preferenceStyle,
                android.R.attr.preferenceStyle,
            ),
            0,
        ) {
            defaultValue = when {
                hasValue(R.styleable.RadioButtonPreference_defaultValue) ->
                    getBoolean(R.styleable.RadioButtonPreference_defaultValue, false)
                hasValue(R.styleable.RadioButtonPreference_android_defaultValue) ->
                    getBoolean(R.styleable.RadioButtonPreference_android_defaultValue, false)
                else -> false
            }
        }
    }

    override fun addToRadioGroup(radioButton: GroupableRadioButton) {
        radioGroups.add(radioButton)
    }

    fun onClickListener(listener: (() -> Unit)) {
        clickListener = listener
    }

    override fun setEnabled(enabled: Boolean) {
        super.setEnabled(enabled)
        if (!enabled) {
            summaryView?.alpha = HALF_ALPHA
            titleView?.alpha = HALF_ALPHA
        } else {
            summaryView?.alpha = FULL_ALPHA
            titleView?.alpha = FULL_ALPHA
        }
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

    override fun updateRadioValue(isChecked: Boolean) {
        persistBoolean(isChecked)
        radioButton?.isChecked = isChecked
        preferences.edit().putBoolean(key, isChecked)
            .apply()
        onPreferenceChangeListener?.onPreferenceChange(this, isChecked)
    }

    private fun bindRadioButton(holder: PreferenceViewHolder) {
        radioButton = holder.findViewById(R.id.radio_button) as RadioButton
        radioButton?.isChecked = preferences.getBoolean(key, defaultValue)
        radioButton?.setStartCheckedIndicator()
    }

    private fun toggleRadioGroups() {
        if (radioButton?.isChecked == true) {
            radioGroups.uncheckAll()
        }
    }

    private fun bindTitle(holder: PreferenceViewHolder) {
        titleView = holder.findViewById(R.id.title) as TextView
        titleView?.alpha = if (isEnabled) FULL_ALPHA else HALF_ALPHA

        if (!title.isNullOrEmpty()) {
            titleView?.text = title
        }
    }

    private fun bindSummaryView(holder: PreferenceViewHolder) {
        summaryView = holder.findViewById(R.id.widget_summary) as TextView

        summaryView?.alpha = if (isEnabled) FULL_ALPHA else HALF_ALPHA
        summaryView?.let {
            if (!summary.isNullOrEmpty()) {
                it.text = if (shouldSummaryBeParsedAsHtmlContent) {
                    HtmlCompat.fromHtml(summary.toString(), HtmlCompat.FROM_HTML_MODE_COMPACT)
                } else {
                    summary
                }

                it.isVisible = true
            } else {
                it.isVisible = false
            }
        }
    }

    /**
     * In devices with Android 6, when we use android:button="@null" android:drawableStart doesn't work via xml
     * as a result we have to apply it programmatically. More info about this issue https://github.com/mozilla-mobile/fenix/issues/1414
     */
    private fun RadioButton.setStartCheckedIndicator() {
        val attr = context.theme.resolveAttribute(android.R.attr.listChoiceIndicatorSingle)
        val buttonDrawable = AppCompatResources.getDrawable(context, attr)
        buttonDrawable?.apply {
            setBounds(0, 0, intrinsicWidth, intrinsicHeight)
        }
        putCompoundDrawablesRelative(start = buttonDrawable)
    }

    companion object {
        const val HALF_ALPHA = 0.5F
        const val FULL_ALPHA = 1F
    }
}
