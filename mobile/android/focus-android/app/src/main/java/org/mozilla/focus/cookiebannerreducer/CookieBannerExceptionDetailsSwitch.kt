/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.cookiebannerreducer

import android.content.Context
import android.util.AttributeSet
import android.view.LayoutInflater
import androidx.constraintlayout.widget.ConstraintLayout
import org.mozilla.focus.R
import org.mozilla.focus.databinding.SwitchWithDescriptionBinding

class CookieBannerExceptionDetailsSwitch @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : ConstraintLayout(context, attrs, defStyleAttr) {

    internal var binding: SwitchWithDescriptionBinding

    init {
        val view =
            LayoutInflater.from(context).inflate(R.layout.switch_with_description, this, true)
        binding = SwitchWithDescriptionBinding.bind(view)
        setTitle()
    }

    private fun setTitle() {
        binding.title.text = context.getString(R.string.cookie_banner_exception_panel_switch_title)
    }
}
