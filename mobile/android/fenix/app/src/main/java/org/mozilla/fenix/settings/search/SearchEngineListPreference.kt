/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.search

import android.content.Context
import android.content.res.Resources
import android.graphics.drawable.BitmapDrawable
import android.util.AttributeSet
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.CompoundButton
import android.widget.LinearLayout
import android.widget.RadioGroup
import androidx.core.view.isVisible
import androidx.navigation.Navigation
import androidx.preference.Preference
import androidx.preference.PreferenceViewHolder
import kotlinx.android.synthetic.main.search_engine_radio_button.view.engine_icon
import kotlinx.android.synthetic.main.search_engine_radio_button.view.engine_text
import kotlinx.android.synthetic.main.search_engine_radio_button.view.overflow_menu
import kotlinx.android.synthetic.main.search_engine_radio_button.view.radio_button
import kotlinx.coroutines.MainScope
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.provider.SearchEngineList
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.searchengine.CustomSearchEngineStore
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getRootView
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.allowUndo
import java.util.Locale

abstract class SearchEngineListPreference @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = android.R.attr.preferenceStyle
) : Preference(context, attrs, defStyleAttr), CompoundButton.OnCheckedChangeListener {

    protected lateinit var searchEngineList: SearchEngineList
    protected var searchEngineGroup: RadioGroup? = null

    protected abstract val itemResId: Int

    init {
        layoutResource = R.layout.preference_search_engine_chooser
    }

    override fun onBindViewHolder(holder: PreferenceViewHolder?) {
        super.onBindViewHolder(holder)
        searchEngineGroup = holder!!.itemView.findViewById(R.id.search_engine_group)
        reload(searchEngineGroup!!.context)
    }

    fun reload(context: Context) {
        searchEngineList = context.components.search.provider.installedSearchEngines(context)
        refreshSearchEngineViews(context)
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

        val defaultEngine = context.components.search.provider.getDefaultEngine(context).identifier
        val selectedEngine = (searchEngineList.list.find {
            it.identifier == defaultEngine
        } ?: searchEngineList.list.first()).identifier

        context.components.search.searchEngineManager.defaultSearchEngine =
            searchEngineList.list.find {
                it.identifier == selectedEngine
            }

        searchEngineGroup!!.removeAllViews()

        val layoutInflater = LayoutInflater.from(context)
        val layoutParams = ViewGroup.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT
        )

        val setupSearchEngineItem: (Int, SearchEngine) -> Unit = { index, engine ->
            val engineId = engine.identifier
            val engineItem = makeButtonFromSearchEngine(
                engine = engine,
                layoutInflater = layoutInflater,
                res = context.resources,
                allowDeletion = searchEngineList.list.size > 1
            )

            engineItem.id = index + (searchEngineList.default?.let { 1 } ?: 0)
            engineItem.tag = engineId
            if (engineId == selectedEngine) {
                updateDefaultItem(engineItem.radio_button)
                /* #11465 -> radio_button.isChecked = true does not trigger
                * onSearchEngineSelected because searchEngineGroup has null views at that point.
                * So we trigger it here.*/
                onSearchEngineSelected(engine)
            }
            searchEngineGroup!!.addView(engineItem, layoutParams)
        }

        searchEngineList.default?.apply {
            setupSearchEngineItem(0, this)
        }

        searchEngineList.list
            .filter { it.identifier != searchEngineList.default?.identifier }
            .sortedBy { it.name.toLowerCase(Locale.getDefault()) }
            .forEachIndexed(setupSearchEngineItem)
    }

    private fun makeButtonFromSearchEngine(
        engine: SearchEngine,
        layoutInflater: LayoutInflater,
        res: Resources,
        allowDeletion: Boolean
    ): View {
        val isCustomSearchEngine =
            CustomSearchEngineStore.isCustomSearchEngine(context, engine.identifier)

        val wrapper = layoutInflater.inflate(itemResId, null) as LinearLayout
        wrapper.setOnClickListener { wrapper.radio_button.isChecked = true }
        wrapper.radio_button.setOnCheckedChangeListener(this)
        wrapper.engine_text.text = engine.name
        wrapper.overflow_menu.isVisible = allowDeletion || isCustomSearchEngine
        wrapper.overflow_menu.setOnClickListener {
            SearchEngineMenu(
                context = context,
                allowDeletion = allowDeletion,
                isCustomSearchEngine = isCustomSearchEngine,
                onItemTapped = {
                    when (it) {
                        is SearchEngineMenu.Item.Edit -> editCustomSearchEngine(engine)
                        is SearchEngineMenu.Item.Delete -> deleteSearchEngine(
                            context,
                            engine,
                            isCustomSearchEngine
                        )
                    }
                }
            ).menuBuilder.build(context).show(wrapper.overflow_menu)
        }
        val iconSize = res.getDimension(R.dimen.preference_icon_drawable_size).toInt()
        val engineIcon = BitmapDrawable(res, engine.icon)
        engineIcon.setBounds(0, 0, iconSize, iconSize)
        wrapper.engine_icon.setImageDrawable(engineIcon)
        return wrapper
    }

    override fun onCheckedChanged(buttonView: CompoundButton, isChecked: Boolean) {
        searchEngineList.list.forEach { engine ->
            val wrapper: LinearLayout =
                searchEngineGroup?.findViewWithTag(engine.identifier) ?: return

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

    private fun editCustomSearchEngine(engine: SearchEngine) {
        val directions = SearchEngineFragmentDirections
            .actionSearchEngineFragmentToEditCustomSearchEngineFragment(engine.identifier)
        Navigation.findNavController(searchEngineGroup!!).navigate(directions)
    }

    private fun deleteSearchEngine(
        context: Context,
        engine: SearchEngine,
        isCustomSearchEngine: Boolean
    ) {
        val isDefaultEngine = engine == context.components.search.provider.getDefaultEngine(context)
        val initialEngineList = searchEngineList.copy()
        val initialDefaultEngine = searchEngineList.default

        context.components.search.provider.uninstallSearchEngine(
            context,
            engine,
            isCustomSearchEngine
        )

        MainScope().allowUndo(
            view = context.getRootView()!!,
            message = context
                .getString(R.string.search_delete_search_engine_success_message, engine.name),
            undoActionTitle = context.getString(R.string.snackbar_deleted_undo),
            onCancel = {
                context.components.search.provider.installSearchEngine(
                    context,
                    engine,
                    isCustomSearchEngine
                )

                searchEngineList = initialEngineList.copy(
                    default = initialDefaultEngine
                )

                refreshSearchEngineViews(context)
            },
            operation = {
                if (isDefaultEngine) {
                    context.settings().defaultSearchEngineName = context
                        .components
                        .search
                        .provider
                        .getDefaultEngine(context)
                        .name
                }
                if (isCustomSearchEngine) {
                    context.components.analytics.metrics.track(Event.CustomEngineDeleted)
                }
                refreshSearchEngineViews(context)
            }
        )

        searchEngineList = searchEngineList.copy(
            list = searchEngineList.list.filter {
                it.identifier != engine.identifier
            },
            default = if (searchEngineList.default?.identifier == engine.identifier) {
                null
            } else {
                searchEngineList.default
            }
        )

        refreshSearchEngineViews(context)
    }
}
