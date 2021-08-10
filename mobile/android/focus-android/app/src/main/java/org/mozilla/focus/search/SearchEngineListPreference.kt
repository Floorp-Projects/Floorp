/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search

import android.content.Context
import android.content.res.Resources
import android.graphics.drawable.BitmapDrawable
import android.util.AttributeSet
import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.CompoundButton
import android.widget.RadioGroup
import androidx.preference.Preference
import androidx.preference.PreferenceViewHolder
import androidx.recyclerview.widget.RecyclerView
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.searchEngines
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import org.mozilla.focus.R
import org.mozilla.focus.ext.components
import kotlin.coroutines.CoroutineContext

abstract class SearchEngineListPreference @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : Preference(context, attrs, defStyleAttr), CoroutineScope {

    private val job = Job()
    override val coroutineContext: CoroutineContext
        get() = job + Dispatchers.Main
    protected var searchEngines: List<SearchEngine> = emptyList()
    protected var searchEngineGroup: RadioGroup? = null

    protected abstract val itemResId: Int

    init {
        layoutResource = R.layout.preference_search_engine_chooser
    }

    override fun onBindViewHolder(holder: PreferenceViewHolder?) {
        super.onBindViewHolder(holder)
        searchEngineGroup = holder!!.itemView.findViewById(R.id.search_engine_group)
        val context = searchEngineGroup!!.context

        searchEngines = context.components.store.state.search.searchEngines

        refreshSearchEngineViews(context)
    }

    override fun onDetached() {
        job.cancel()
        super.onDetached()
    }

    protected abstract fun updateDefaultItem(defaultButton: CompoundButton)

    fun refetchSearchEngines() {
        searchEngines = context.components.store.state.search.searchEngines
        refreshSearchEngineViews(this@SearchEngineListPreference.context)
    }

    private fun refreshSearchEngineViews(context: Context) {
        if (searchEngineGroup == null) {
            // We want to refresh the search engine list of this preference in onResume,
            // but the first time this preference is created onResume is called before onCreateView
            // so searchEngineGroup is not set yet.
            return
        }

        val defaultSearchEngine =
            context.components.store.state.search.selectedOrDefaultSearchEngine?.id

        searchEngineGroup!!.removeAllViews()

        val layoutInflater = LayoutInflater.from(context)
        val layoutParams = RecyclerView.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT
        )

        for (i in searchEngines.indices) {
            val engine = searchEngines[i]
            val engineId = engine.id
            val engineItem = makeButtonFromSearchEngine(engine, layoutInflater, context.resources)
            engineItem.id = i
            engineItem.tag = engineId
            if (engineId == defaultSearchEngine) {
                updateDefaultItem(engineItem)
            }
            searchEngineGroup!!.addView(engineItem, layoutParams)
        }
    }

    private fun makeButtonFromSearchEngine(
        engine: SearchEngine,
        layoutInflater: LayoutInflater,
        res: Resources
    ): CompoundButton {
        val buttonItem = layoutInflater.inflate(itemResId, null) as CompoundButton
        buttonItem.text = engine.name
        val iconSize = res.getDimension(R.dimen.preference_icon_drawable_size).toInt()
        val engineIcon = BitmapDrawable(res, engine.icon)
        engineIcon.setBounds(0, 0, iconSize, iconSize)
        val drawables = buttonItem.compoundDrawables
        buttonItem.setCompoundDrawables(engineIcon, null, drawables[2], null)
        return buttonItem
    }
}
