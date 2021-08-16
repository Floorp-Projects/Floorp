/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.content.Context
import android.util.AttributeSet
import android.view.LayoutInflater
import android.widget.TextView
import androidx.appcompat.content.res.AppCompatResources
import androidx.appcompat.widget.SwitchCompat
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.content.withStyledAttributes
import mozilla.components.support.ktx.android.view.putCompoundDrawablesRelativeWithIntrinsicBounds
import org.mozilla.fenix.R

class SwitchWithDescription @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : ConstraintLayout(context, attrs, defStyleAttr) {

    lateinit var switchWidget: SwitchCompat
    lateinit var trackingProtectionCategoryTitle: TextView
    lateinit var trackingProtectionCategoryItemDescription: TextView

    init {
        LayoutInflater.from(context).inflate(R.layout.switch_with_description, this, true)

        context.withStyledAttributes(attrs, R.styleable.SwitchWithDescription, defStyleAttr, 0) {
            val id = getResourceId(
                R.styleable.SwitchWithDescription_switchIcon,
                R.drawable.ic_tracking_protection
            )
            switchWidget = findViewById(R.id.switch_widget)
            trackingProtectionCategoryTitle = findViewById(R.id.trackingProtectionCategoryTitle)
            trackingProtectionCategoryItemDescription = findViewById(R.id.trackingProtectionCategoryItemDescription)
            switchWidget.putCompoundDrawablesRelativeWithIntrinsicBounds(
                start = AppCompatResources.getDrawable(context, id)
            )
            trackingProtectionCategoryTitle.text = resources.getString(
                getResourceId(
                    R.styleable.SwitchWithDescription_switchTitle,
                    R.string.preference_enhanced_tracking_protection
                )
            )
            trackingProtectionCategoryItemDescription.text = resources.getString(
                getResourceId(
                    R.styleable.SwitchWithDescription_switchDescription,
                    R.string.preference_enhanced_tracking_protection_explanation
                )
            )
        }
    }
}
