package org.mozilla.fenix.onboarding

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.content.Context
import android.util.AttributeSet
import androidx.appcompat.widget.AppCompatRadioButton
import org.mozilla.fenix.R
import org.mozilla.fenix.utils.Settings

class OnboardingRadioButton(context: Context, attrs: AttributeSet) : AppCompatRadioButton(context, attrs) {
    private val radioGroups = mutableListOf<OnboardingRadioButton>()
    private var clickListener: (() -> Unit)? = null
    var key: Int = 0

    init {
        attrs.let {
            context.theme.obtainStyledAttributes(
                it,
                R.styleable.OnboardingRadioButton,
                0, 0
            ).apply {
                try {
                    key = getResourceId(
                        R.styleable.OnboardingRadioButton_onboardingKey, 0
                    )
                } finally {
                    recycle()
                }
            }
        }
    }

    fun addToRadioGroup(radioButton: OnboardingRadioButton) {
        radioGroups.add(radioButton)
    }

    fun onClickListener(listener: (() -> Unit)) {
        clickListener = listener
    }

    init {
        setOnClickListener {
            updateRadioValue(true)
            toggleRadioGroups()
            clickListener?.invoke()
        }
    }

    private fun updateRadioValue(isChecked: Boolean) {
        this.isChecked = isChecked
        Settings.getInstance(context).preferences.edit().putBoolean(context.getString(key), isChecked)
            .apply()
    }

    private fun toggleRadioGroups() {
        if (this.isChecked) {
            radioGroups.forEach { it.updateRadioValue(false) }
        }
    }
}
