/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search

import android.content.Context
import android.util.AttributeSet
import android.widget.CompoundButton
import android.widget.RadioGroup
import androidx.preference.PreferenceViewHolder
import mozilla.components.browser.state.search.SearchEngine
import org.mozilla.focus.GleanMetrics.SearchEngines
import org.mozilla.focus.R
import org.mozilla.focus.ext.components

private const val ENGINE_TYPE_CUSTOM = "custom"
private const val ENGINE_TYPE_BUNDLED = "bundled"

class RadioSearchEngineListPreference : SearchEngineListPreference, RadioGroup.OnCheckedChangeListener {

    override val itemResId: Int
        get() = R.layout.search_engine_radio_button

    @Suppress("unused")
    constructor(context: Context, attrs: AttributeSet) : super(context, attrs)

    @Suppress("unused")
    constructor(context: Context, attrs: AttributeSet, defStyleAttr: Int) : super(context, attrs, defStyleAttr)

    override fun onBindViewHolder(holder: PreferenceViewHolder) {
        super.onBindViewHolder(holder)
        searchEngineGroup!!.setOnCheckedChangeListener(this)
    }

    override fun updateDefaultItem(defaultButton: CompoundButton) {
        defaultButton.isChecked = true
    }

    override fun onCheckedChanged(group: RadioGroup, checkedId: Int) {
        val selectedEngine = group.getChildAt(checkedId) ?: return

        // check if the corresponding button was pressed or a11y focused.
        val hasProperState = selectedEngine.isPressed || selectedEngine.isAccessibilityFocused

        /* onCheckedChanged is called intermittently before the search engine table is full, so we
           must check these conditions to prevent crashes and inconsistent states. */
        if (group.childCount != searchEngines.count() || !hasProperState) {
            return
        }

        val newDefaultEngine = searchEngines[checkedId]

        context.components.searchUseCases.selectSearchEngine(newDefaultEngine)

        val source = if (newDefaultEngine.type == SearchEngine.Type.CUSTOM) {
            ENGINE_TYPE_CUSTOM
        } else {
            ENGINE_TYPE_BUNDLED
        }

        SearchEngines.setDefault.record(SearchEngines.SetDefaultExtra(source))
    }
}
