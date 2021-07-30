package org.mozilla.focus.settings.privacy

import android.content.Context
import android.util.AttributeSet
import androidx.appcompat.widget.SwitchCompat
import androidx.core.content.edit
import androidx.core.content.withStyledAttributes
import androidx.preference.PreferenceManager
import org.mozilla.focus.R

class PreferenceSwitch(
    context: Context,
    attrs: AttributeSet
) : SwitchCompat(context, attrs) {

    private var clickListener: (() -> Unit)? = null
    var key: Int = 0
    var title: Int = 0
    var description: Int = 0

    init {
        context.withStyledAttributes(
            attrs,
            R.styleable.PreferenceSwitch,
            0, 0
        ) {
            key = getResourceId(R.styleable.PreferenceSwitch_preferenceKey, 0)
            title = getResourceId(R.styleable.PreferenceSwitch_preferenceKeyTitle, 0)
            description =
                getResourceId(R.styleable.PreferenceSwitch_preferenceKeyDescription, 0)
        }
    }

    init {
        setInitialValue()
        this.setOnCheckedChangeListener(null)
        setOnClickListener {
            togglePreferenceValue(this.isChecked)
            clickListener?.invoke()
        }

        if (title != 0) {
            this.text = context.getString(title)
        }
    }

    private fun setInitialValue() {
        this.isChecked = PreferenceManager.getDefaultSharedPreferences(context)
            .getBoolean(context.getString(key), false)
    }

    fun onClickListener(listener: () -> Unit) {
        clickListener = listener
    }

    private fun togglePreferenceValue(isChecked: Boolean) {
        this.isChecked = isChecked

        PreferenceManager.getDefaultSharedPreferences(context).edit {
            putBoolean(context.getString(key), isChecked)
        }
    }
}
