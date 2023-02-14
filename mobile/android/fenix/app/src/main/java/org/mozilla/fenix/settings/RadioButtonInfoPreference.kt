/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import android.util.AttributeSet
import android.widget.ImageView
import androidx.preference.PreferenceViewHolder
import org.mozilla.fenix.R

class RadioButtonInfoPreference @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
) : RadioButtonPreference(context, attrs) {
    private var infoClickListener: (() -> Unit)? = null
    private var infoView: ImageView? = null
    var contentDescription: String? = null

    fun onInfoClickListener(listener: (() -> Unit)) {
        infoClickListener = listener
    }

    override fun setEnabled(enabled: Boolean) {
        super.setEnabled(enabled)
        infoView?.alpha = if (enabled) FULL_ALPHA else HALF_ALPHA
        infoView?.isEnabled = enabled
    }

    init {
        layoutResource = R.layout.preference_widget_radiobutton_with_info
    }

    override fun onBindViewHolder(holder: PreferenceViewHolder) {
        super.onBindViewHolder(holder)
        infoView = holder.findViewById(R.id.info_button) as ImageView
        infoView?.setOnClickListener {
            infoClickListener?.invoke()
        }
        infoView?.alpha = if (isEnabled) FULL_ALPHA else HALF_ALPHA
        contentDescription?.let { infoView?.contentDescription = it }
    }
}
