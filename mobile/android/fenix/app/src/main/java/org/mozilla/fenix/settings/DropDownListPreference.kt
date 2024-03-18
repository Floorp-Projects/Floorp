/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import android.util.AttributeSet
import android.widget.ArrayAdapter
import androidx.preference.DropDownPreference
import androidx.preference.ListPreference
import org.mozilla.fenix.R

open class DropDownListPreference @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
) : DropDownPreference(context, attrs) {

    init {
        layoutResource = R.layout.dropdown_preference_etp
    }

    override fun createAdapter(): ArrayAdapter<Any> {
        return ArrayAdapter(context, R.layout.etp_dropdown_item)
    }
}

/**
 * Return the (human-readable) entry that is matched to the (backing key) entryValue.
 *
 * E.g.
 *   entryValues == listOf("private", "normal")
 *   entries == listOf("Use private mode", "Use normal mode")
 *
 *   findEntry("private) == "Use Private Mode"
 */
fun ListPreference.findEntry(key: Any?): CharSequence? {
    if (key !is String) return null

    val index = entryValues.indexOf(key)
    return this.entries.getOrNull(index)
}
