/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.content.Context
import android.util.AttributeSet
import android.view.LayoutInflater
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.content.withStyledAttributes
import kotlinx.android.synthetic.main.switch_with_description.view.*
import kotlinx.android.synthetic.main.tracking_protection_category.view.switchItemDescription
import kotlinx.android.synthetic.main.tracking_protection_category.view.switchItemTitle
import org.mozilla.fenix.R

class SwitchWithDescription @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : ConstraintLayout(context, attrs) {

    init {
        LayoutInflater.from(context).inflate(R.layout.switch_with_description, this, true)

        context.withStyledAttributes(attrs, R.styleable.SwitchWithDescription, defStyleAttr, 0) {
            val id = getResourceId(
                R.styleable.SwitchWithDescription_switchIcon,
                R.drawable.ic_tracking_protection
            )
            switch_widget?.setCompoundDrawablesWithIntrinsicBounds(
                resources.getDrawable(
                    id,
                    context.theme
                ), null, null, null
            )
            switchItemTitle?.text = resources.getString(
                getResourceId(
                    R.styleable.SwitchWithDescription_switchTitle,
                    R.string.preference_enhanced_tracking_protection
                )
            )
            switchItemDescription?.text = resources.getString(
                getResourceId(
                    R.styleable.SwitchWithDescription_switchDescription,
                    R.string.preference_enhanced_tracking_protection_explanation
                )
            )
        }
    }
}
