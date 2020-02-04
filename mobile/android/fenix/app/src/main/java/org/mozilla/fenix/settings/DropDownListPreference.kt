/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import android.util.AttributeSet
import android.widget.ArrayAdapter
import androidx.preference.DropDownPreference
import org.mozilla.fenix.R

class DropDownListPreference @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : DropDownPreference(context, attrs) {

    init {
        layoutResource = R.layout.dropdown_preference_etp
    }

    override fun createAdapter(): ArrayAdapter<Any> {
        return ArrayAdapter(context, R.layout.etp_dropdown_item)
    }
}
