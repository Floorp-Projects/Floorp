/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.content.Context
import android.util.AttributeSet
import android.view.LayoutInflater
import androidx.appcompat.content.res.AppCompatResources
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.content.withStyledAttributes
import kotlinx.android.synthetic.main.tracking_protection_category.view.*
import org.mozilla.fenix.R

class TrackingProtectionCategoryItem @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : ConstraintLayout(context, attrs, defStyleAttr) {
    init {
        LayoutInflater.from(context).inflate(R.layout.tracking_protection_category, this, true)

        context.withStyledAttributes(
            attrs,
            R.styleable.TrackingProtectionCategory,
            defStyleAttr,
            0
        ) {
            val id = getResourceId(
                R.styleable.TrackingProtectionCategory_categoryItemIcon,
                R.drawable.ic_cryptominers
            )
            trackingProtectionCategoryIcon?.background = AppCompatResources.getDrawable(context, id)
            trackingProtectionCategoryTitle?.text = resources.getString(
                getResourceId(
                    R.styleable.TrackingProtectionCategory_categoryItemTitle,
                    R.string.etp_cookies_title
                )
            )
            trackingProtectionCategoryItemDescription?.text = resources.getString(
                getResourceId(
                    R.styleable.TrackingProtectionCategory_categoryItemDescription,
                    R.string.etp_cookies_description
                )
            )
        }
    }
}
