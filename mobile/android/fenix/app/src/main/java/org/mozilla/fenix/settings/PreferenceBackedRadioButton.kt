/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import android.util.AttributeSet
import android.widget.CompoundButton.OnCheckedChangeListener
import androidx.annotation.VisibleForTesting
import androidx.appcompat.widget.AppCompatRadioButton
import androidx.core.content.withStyledAttributes
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.settings

/**
 * [AppCompatRadioButton] backed by a boolean `SharedPreference`.
 *
 * Whenever this button is initialized it will attempt to set itself as checked with the
 * current value of `R.styleable.PreferenceBackedRadioButton_preferenceKey` defaulting to
 * `R.styleable.PreferenceBackedRadioButton_preferenceKeyDefaultValue` if there is no value set
 * for the indicated `SharedPreference`.
 *
 * Whenever the radio button is enabled or disabled this will be persisted in a `SharedPreference`
 * with the name indicated in `R.styleable.PreferenceBackedRadioButton_preferenceKey` .
 */
class PreferenceBackedRadioButton @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = R.attr.radioButtonStyle,
) : AppCompatRadioButton(context, attrs, defStyleAttr) {
    @VisibleForTesting
    internal var externalOnCheckedChangeListener: OnCheckedChangeListener? = null

    @VisibleForTesting
    internal var backingPreferenceName: String? = null

    @VisibleForTesting
    internal var backingPreferenceDefaultValue: Boolean = false

    private val internalOnCheckedChangeListener = OnCheckedChangeListener { buttonView, isChecked ->
        backingPreferenceName?.let {
            context.settings().preferences.edit().putBoolean(it, isChecked).apply()
        }

        externalOnCheckedChangeListener?.onCheckedChanged(buttonView, isChecked)
    }

    init {
        context.withStyledAttributes(attrs, R.styleable.PreferenceBackedRadioButton, defStyleAttr, 0) {
            backingPreferenceName = this.getString(R.styleable.PreferenceBackedRadioButton_preferenceKey)
            backingPreferenceDefaultValue = getBoolean(
                R.styleable.PreferenceBackedRadioButton_preferenceKeyDefaultValue,
                false,
            )
        }

        isChecked = context.settings().preferences.getBoolean(backingPreferenceName, backingPreferenceDefaultValue)

        super.setOnCheckedChangeListener(internalOnCheckedChangeListener)
    }

    override fun setOnCheckedChangeListener(listener: OnCheckedChangeListener?) {
        externalOnCheckedChangeListener = listener
    }

    override fun setEnabled(enabled: Boolean) {
        super.setEnabled(enabled)

        if (enabled) {
            isChecked = context.settings().preferences.getBoolean(backingPreferenceName, backingPreferenceDefaultValue)
        } else {
            context.settings().preferences.edit().remove(backingPreferenceName).apply()
        }
    }
}
