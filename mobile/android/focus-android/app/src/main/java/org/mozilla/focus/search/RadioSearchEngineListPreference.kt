/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search

import android.content.Context
import android.support.v7.preference.PreferenceViewHolder
import android.util.AttributeSet
import android.widget.CompoundButton
import android.widget.RadioGroup
import org.mozilla.focus.R
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.utils.Settings

class RadioSearchEngineListPreference : SearchEngineListPreference, RadioGroup.OnCheckedChangeListener {

    override val itemResId: Int
        get() = R.layout.search_engine_radio_button

    @Suppress("unused")
    constructor(context: Context, attrs: AttributeSet) : super(context, attrs)

    @Suppress("unused")
    constructor(context: Context, attrs: AttributeSet, defStyleAttr: Int) : super(context, attrs, defStyleAttr)

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
        if (group.childCount != searchEngines.count() || !group.getChildAt(checkedId).isPressed) {
            return
        }

        val newDefaultEngine = searchEngines[checkedId]
        Settings.getInstance(group.context).setDefaultSearchEngineByName(newDefaultEngine.name)
        val source = if (CustomSearchEngineStore.isCustomSearchEngine(newDefaultEngine.identifier, context))
            CustomSearchEngineStore.ENGINE_TYPE_CUSTOM
        else
            CustomSearchEngineStore.ENGINE_TYPE_BUNDLED
        TelemetryWrapper.setDefaultSearchEngineEvent(source)
    }
}
