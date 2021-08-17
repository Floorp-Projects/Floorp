/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.content.Context
import android.util.AttributeSet
import android.view.LayoutInflater
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.content.withStyledAttributes
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.TrackingProtectionCategoryBinding

class TrackingProtectionCategoryItem @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : ConstraintLayout(context, attrs, defStyleAttr) {
    init {
        val binding = TrackingProtectionCategoryBinding.inflate(
            LayoutInflater.from(context),
            this
        )

        context.withStyledAttributes(
            attrs,
            R.styleable.TrackingProtectionCategory,
            defStyleAttr,
            0
        ) {
            binding.trackingProtectionCategoryTitle.text = resources.getString(
                getResourceId(
                    R.styleable.TrackingProtectionCategory_categoryItemTitle,
                    R.string.etp_cookies_title
                )
            )
            binding.trackingProtectionCategoryItemDescription.text = resources.getString(
                getResourceId(
                    R.styleable.TrackingProtectionCategory_categoryItemDescription,
                    R.string.etp_cookies_description
                )
            )
        }
    }
}
