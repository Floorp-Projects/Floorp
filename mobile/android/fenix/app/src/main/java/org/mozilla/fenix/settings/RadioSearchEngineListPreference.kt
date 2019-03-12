/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import android.util.AttributeSet
import android.widget.CompoundButton
import android.widget.RadioGroup
import androidx.preference.PreferenceViewHolder
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.utils.Settings

class RadioSearchEngineListPreference : SearchEngineListPreference,
    RadioGroup.OnCheckedChangeListener {

    override val itemResId: Int
        get() = R.layout.search_engine_radio_button

    constructor(context: Context, attrs: AttributeSet) : super(context, attrs)

    constructor(context: Context, attrs: AttributeSet, defStyleAttr: Int) : super(
        context,
        attrs,
        defStyleAttr
    )

    override fun onBindViewHolder(holder: PreferenceViewHolder?) {
        super.onBindViewHolder(holder)
        searchEngineGroup!!.setOnCheckedChangeListener(this)
    }

    override fun updateDefaultItem(defaultButton: CompoundButton) {
        defaultButton.isChecked = true
    }

    override fun onCheckedChanged(group: RadioGroup, checkedId: Int) {
        /* onCheckedChanged is called intermittently before the search engine table is full, so we
           must check these conditions to prevent crashes and inconsistent states. */
        if (group.childCount != searchEngines.count() || group.getChildAt(checkedId) == null ||
            !group.getChildAt(checkedId).isPressed
        ) {
            return
        }

        val newDefaultEngine = searchEngines[checkedId]
        context.components.search.searchEngineManager.defaultSearchEngine = newDefaultEngine
        Settings.getInstance(group.context).setDefaultSearchEngineByName(newDefaultEngine.name)
    }
}
