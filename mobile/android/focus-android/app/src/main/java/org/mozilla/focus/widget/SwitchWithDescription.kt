/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.widget

import android.content.Context
import android.util.AttributeSet
import android.view.LayoutInflater
import androidx.appcompat.content.res.AppCompatResources
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.content.withStyledAttributes
import mozilla.components.support.ktx.android.view.putCompoundDrawablesRelativeWithIntrinsicBounds
import org.mozilla.focus.R
import org.mozilla.focus.databinding.SwitchWithDescriptionBinding

class SwitchWithDescription @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : ConstraintLayout(context, attrs, defStyleAttr) {

    internal var binding: SwitchWithDescriptionBinding

    init {
        val view =
            LayoutInflater.from(context).inflate(R.layout.switch_with_description, this, true)
        binding = SwitchWithDescriptionBinding.bind(view)

        context.withStyledAttributes(attrs, R.styleable.SwitchWithDescription, defStyleAttr, 0) {
            val icon = getResourceId(
                R.styleable.SwitchWithDescription_switchIcon,
                R.drawable.mozac_ic_shield
            )
            updateIcon(icon)

            updateTitle(
                resources.getString(
                    getResourceId(
                        R.styleable.SwitchWithDescription_switchTitle,
                        R.string.enhanced_tracking_protection
                    )
                )
            )

            updateDescription(
                resources.getString(
                    getResourceId(
                        R.styleable.SwitchWithDescription_switchDescription,
                        R.string.enhanced_tracking_protection_state_on
                    )
                )
            )
        }
    }

    private fun updateTitle(title: String) {
        binding.title.text = title
    }

    internal fun updateDescription(description: String) {
        binding.description.text = description
    }

    internal fun updateIcon(icon: Int) {
        binding.switchWidget.putCompoundDrawablesRelativeWithIntrinsicBounds(
            start = AppCompatResources.getDrawable(context, icon)
        )
    }
}
