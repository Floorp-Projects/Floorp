/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search

import android.content.Context
import android.util.AttributeSet
import android.widget.CompoundButton
import androidx.preference.PreferenceViewHolder
import org.mozilla.focus.R
import org.mozilla.focus.utils.asActivity
import java.util.HashSet

class MultiselectSearchEngineListPreference(context: Context, attrs: AttributeSet) :
    SearchEngineListPreference(context, attrs) {

    override val itemResId: Int
        get() = R.layout.search_engine_checkbox_button

    val checkedEngineIds: Set<String>
        get() {
            val engineIdSet = HashSet<String>()

            for (i in 0 until searchEngineGroup!!.childCount) {
                val engineButton = searchEngineGroup!!.getChildAt(i) as CompoundButton
                if (engineButton.isChecked) {
                    engineIdSet.add(engineButton.tag as String)
                }
            }
            return engineIdSet
        }

    override fun onBindViewHolder(holder: PreferenceViewHolder?) {
        super.onBindViewHolder(holder)
        this.bindEngineCheckboxesToMenu()
    }

    override fun updateDefaultItem(defaultButton: CompoundButton) {
        defaultButton.isClickable = false
        // Showing the default engine as disabled requires a StateListDrawable, but since there
        // is no state_clickable and state_enabled seems to require a default drawable state,
        // use state_activated instead to designate the default search engine.
        defaultButton.isActivated = true
    }

    // Whenever an engine is checked or unchecked, we notify the menu
    private fun bindEngineCheckboxesToMenu() {
        for (i in 0 until searchEngineGroup!!.childCount) {
            val engineButton = searchEngineGroup!!.getChildAt(i) as CompoundButton
            engineButton.setOnCheckedChangeListener { _, _ ->
                val context = context
                context?.asActivity()?.invalidateOptionsMenu()
            }
        }
    }

    fun atLeastOneEngineChecked(): Boolean {
        for (i in 0 until searchEngineGroup!!.childCount) {
            val engineButton = searchEngineGroup!!.getChildAt(i) as CompoundButton
            if (engineButton.isChecked) {
                return true
            }
        }
        return false
    }
}
