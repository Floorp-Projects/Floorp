/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.content.Context
import android.util.AttributeSet
import android.view.LayoutInflater
import android.widget.CompoundButton
import android.widget.TextView
import androidx.annotation.VisibleForTesting
import androidx.appcompat.content.res.AppCompatResources
import androidx.appcompat.widget.SwitchCompat
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.content.withStyledAttributes
import mozilla.components.support.ktx.android.view.putCompoundDrawablesRelativeWithIntrinsicBounds
import org.mozilla.fenix.R

private const val DEFAULT_DRAWABLE: Int = 0

/**
 * Add a [SwitchCompat] widget with description that will vary depending on switch status.
 */
class SwitchWithDescription @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : ConstraintLayout(context, attrs, defStyleAttr) {

    private lateinit var switchWidget: SwitchCompat
    private lateinit var titleWidget: TextView
    private lateinit var descriptionWidget: TextView
    private lateinit var descriptionOn: String
    private lateinit var descriptionOff: String
    private var iconOn: Int = 0
    private var iconOff: Int = 0
    private var shouldShowIcons: Boolean = true

    init {
        LayoutInflater.from(context).inflate(R.layout.switch_with_description, this, true)

        context.withStyledAttributes(attrs, R.styleable.SwitchWithDescription, defStyleAttr, 0) {
            switchWidget = findViewById(R.id.switch_widget)
            titleWidget = findViewById(R.id.switch_with_description_title)
            descriptionWidget = findViewById(R.id.switch_with_description_description)

            switchWidget.setOnCheckedChangeListener { _, isChecked ->
                onSwitchChange(isChecked)
            }

            iconOn = getResourceId(
                R.styleable.SwitchWithDescription_switchIconOn,
                DEFAULT_DRAWABLE,
            )
            iconOff = getResourceId(
                R.styleable.SwitchWithDescription_switchIconOff,
                DEFAULT_DRAWABLE,
            )

            shouldShowIcons = getBoolean(
                R.styleable.SwitchWithDescription_switchShowIcon,
                true,
            )

            descriptionOn = resources.getString(
                getResourceId(
                    R.styleable.SwitchWithDescription_switchDescriptionOn,
                    R.string.empty_string,
                ),
            )
            descriptionOff = resources.getString(
                getResourceId(
                    R.styleable.SwitchWithDescription_switchDescriptionOff,
                    R.string.empty_string,
                ),
            )

            switchWidget.textOn = descriptionOn
            switchWidget.textOff = descriptionOff

            titleWidget.text = resources.getString(
                getResourceId(
                    R.styleable.SwitchWithDescription_switchTitle,
                    R.string.empty_string,
                ),
            )

            if (shouldShowIcons) {
                switchWidget.putCompoundDrawablesRelativeWithIntrinsicBounds(
                    start = AppCompatResources.getDrawable(context, iconOn),
                )
            }
        }
    }

    /**
     * Add a [CompoundButton.OnCheckedChangeListener] listener to the switch view.
     */
    fun setOnCheckedChangeListener(listener: CompoundButton.OnCheckedChangeListener) {
        switchWidget.setOnCheckedChangeListener { item, isChecked ->
            onSwitchChange(isChecked)
            listener.onCheckedChanged(item, isChecked)
        }
    }

    /**
     * Allows to query switch view isChecked state.
     */
    var isChecked: Boolean
        get() = switchWidget.isChecked
        set(value) {
            switchWidget.isChecked = value
            onSwitchChange(value)
        }

    @VisibleForTesting
    internal fun onSwitchChange(isChecked: Boolean) {
        val newDescription = if (isChecked) descriptionOn else descriptionOff
        val newIcon = if (isChecked) iconOn else iconOff

        if (shouldShowIcons) {
            switchWidget.putCompoundDrawablesRelativeWithIntrinsicBounds(
                start = AppCompatResources.getDrawable(context, newIcon),
            )
        }
        descriptionWidget.text = newDescription
        switchWidget.jumpDrawablesToCurrentState()
    }
}
