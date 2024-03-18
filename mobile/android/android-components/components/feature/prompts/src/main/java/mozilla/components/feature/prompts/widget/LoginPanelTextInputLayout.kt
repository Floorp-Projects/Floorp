/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.widget

import android.content.Context
import android.content.res.ColorStateList
import android.content.res.TypedArray
import android.util.AttributeSet
import androidx.annotation.StyleableRes
import androidx.core.content.ContextCompat
import androidx.core.content.withStyledAttributes
import com.google.android.material.textfield.TextInputLayout
import mozilla.components.feature.prompts.R

internal class LoginPanelTextInputLayout(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : TextInputLayout(context, attrs, defStyleAttr) {
    constructor(context: Context) : this(context, null, 0)

    constructor(context: Context, attrs: AttributeSet? = null) : this(context, attrs, 0)

    init {
        context.withStyledAttributes(
            attrs,
            R.styleable.LoginPanelTextInputLayout,
            defStyleAttr,
            0,
        ) {

            defaultHintTextColor = ColorStateList.valueOf(
                ContextCompat.getColor(
                    context,
                    R.color.mozacBoxStrokeColor,
                ),
            )

            getColorOrNull(R.styleable.LoginPanelTextInputLayout_mozacInputLayoutErrorTextColor)?.let { color ->
                setErrorTextColor(ColorStateList.valueOf(color))
            }

            getColorOrNull(R.styleable.LoginPanelTextInputLayout_mozacInputLayoutErrorIconColor)?.let { color ->
                setErrorIconTintList(ColorStateList.valueOf(color))
            }
        }
    }

    private fun TypedArray.getColorOrNull(@StyleableRes styleableRes: Int): Int? {
        val resourceId = this.getResourceId(styleableRes, 0)
        return if (resourceId > 0) ContextCompat.getColor(context, resourceId) else null
    }
}
