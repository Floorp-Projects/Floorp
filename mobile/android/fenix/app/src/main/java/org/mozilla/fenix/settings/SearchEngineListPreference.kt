/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import android.content.res.Resources
import android.graphics.drawable.BitmapDrawable
import android.util.AttributeSet
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.CompoundButton
import android.widget.RadioGroup
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.content.ContextCompat
import androidx.preference.Preference
import androidx.preference.PreferenceViewHolder
import kotlinx.android.synthetic.main.search_engine_radio_button.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import mozilla.components.browser.search.SearchEngine
import org.mozilla.fenix.R
import org.mozilla.fenix.ThemeManager
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.utils.Settings
import kotlin.coroutines.CoroutineContext

abstract class SearchEngineListPreference @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = android.R.attr.preferenceStyle
) : Preference(context, attrs, defStyleAttr), CompoundButton.OnCheckedChangeListener, CoroutineScope {
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

        searchEngines = context.components.search.searchEngineManager.getSearchEngines(context)
            .sortedBy { it.name }

        refreshSearchEngineViews(context)
    }

    override fun onDetached() {
        job.cancel()
        super.onDetached()
    }

    protected abstract fun onSearchEngineSelected(searchEngine: SearchEngine)
    protected abstract fun updateDefaultItem(defaultButton: CompoundButton)

    private fun refreshSearchEngineViews(context: Context) {
        if (searchEngineGroup == null) {
            // We want to refresh the search engine list of this preference in onResume,
            // but the first time this preference is created onResume is called before onCreateView
            // so searchEngineGroup is not set yet.
            return
        }

        // To get the default search engine we have to pass in a name that doesn't exist
        // https://github.com/mozilla-mobile/android-components/issues/3344
        val defaultSearchEngine = context.components.search.searchEngineManager.getDefaultSearchEngine(
            context,
            THIS_IS_A_HACK_FIX_ME
        )

        val selectedSearchEngine =
            context.components.search.searchEngineManager.getDefaultSearchEngine(
                context,
                Settings.getInstance(context).defaultSearchEngineName
            ).identifier

        searchEngineGroup!!.removeAllViews()

        val layoutInflater = LayoutInflater.from(context)
        val layoutParams = ViewGroup.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT
        )

        val setupSearchEngineItem: (Int, SearchEngine) -> Unit = { index, engine ->
            val engineId = engine.identifier
            val engineItem = makeButtonFromSearchEngine(engine, layoutInflater, context.resources)
            engineItem.id = index
            engineItem.tag = engineId
            if (engineId == selectedSearchEngine) {
                updateDefaultItem(engineItem.radio_button)
            }
            searchEngineGroup!!.addView(engineItem, layoutParams)
        }

        setupSearchEngineItem(0, defaultSearchEngine)

        searchEngines
            .filter { it.identifier != defaultSearchEngine.identifier }
            .forEachIndexed(setupSearchEngineItem)
    }

    private fun makeButtonFromSearchEngine(
        engine: SearchEngine,
        layoutInflater: LayoutInflater,
        res: Resources
    ): View {
        val wrapper = layoutInflater.inflate(itemResId, null) as ConstraintLayout
        wrapper.setOnClickListener { wrapper.radio_button.isChecked = true }
        wrapper.radio_button.setOnCheckedChangeListener(this)
        val buttonItem = wrapper.radio_button
        wrapper.engine_text.text = engine.name
        val iconSize = res.getDimension(R.dimen.preference_icon_drawable_size).toInt()
        val engineIcon = BitmapDrawable(res, engine.icon)
        engineIcon.setBounds(0, 0, iconSize, iconSize)
        wrapper.engine_icon.setImageDrawable(engineIcon)
        val attr =
            ThemeManager.resolveAttribute(android.R.attr.listChoiceIndicatorSingle, context)
        val buttonDrawable = ContextCompat.getDrawable(context, attr)
        buttonDrawable.apply {
            this?.setBounds(0, 0, this.intrinsicWidth, this.intrinsicHeight)
        }
        buttonItem.setCompoundDrawablesRelative(buttonDrawable, null, null, null)
        return wrapper
    }

    override fun onCheckedChanged(buttonView: CompoundButton, isChecked: Boolean) {
        searchEngines.forEach { engine ->
            val wrapper: ConstraintLayout = searchEngineGroup?.findViewWithTag(engine.identifier) ?: return

            when (wrapper.radio_button == buttonView) {
                true -> onSearchEngineSelected(engine)
                false -> {
                    wrapper.radio_button.setOnCheckedChangeListener(null)
                    wrapper.radio_button.isChecked = false
                    wrapper.radio_button.setOnCheckedChangeListener(this)
                }
            }
        }
    }

    companion object {
        private const val THIS_IS_A_HACK_FIX_ME = "."
    }
}
