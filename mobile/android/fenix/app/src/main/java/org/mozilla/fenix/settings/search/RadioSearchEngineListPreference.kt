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
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.SearchState
import mozilla.components.browser.state.state.searchEngines
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.ext.flow
import mozilla.components.support.ktx.android.view.toScope
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getRootView
import org.mozilla.fenix.utils.allowUndo

class RadioSearchEngineListPreference @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = android.R.attr.preferenceStyle
) : Preference(context, attrs, defStyleAttr), CompoundButton.OnCheckedChangeListener {
    val itemResId: Int
        get() = R.layout.search_engine_radio_button

    init {
        layoutResource = R.layout.preference_search_engine_chooser
    }

    override fun onBindViewHolder(holder: PreferenceViewHolder) {
        super.onBindViewHolder(holder)

        subscribeToSearchEngineUpdates(
            context.components.core.store,
            holder.itemView
        )
    }

    @OptIn(ExperimentalCoroutinesApi::class)
    private fun subscribeToSearchEngineUpdates(store: BrowserStore, view: View) = view.toScope().launch {
        store.flow()
            .map { state -> state.search }
            .ifChanged()
            .collect { state -> refreshSearchEngineViews(view, state) }
    }

    private fun refreshSearchEngineViews(view: View, state: SearchState) {
        val searchEngineGroup = view.findViewById<RadioGroup>(R.id.search_engine_group)
        searchEngineGroup!!.removeAllViews()

        val layoutInflater = LayoutInflater.from(context)
        val layoutParams = ViewGroup.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT
        )

        state.searchEngines.forEach { engine ->
            val searchEngineView = makeButtonFromSearchEngine(
                engine = engine,
                layoutInflater = layoutInflater,
                res = context.resources,
                allowDeletion = state.searchEngines.size > 1,
                isSelected = engine == state.selectedOrDefaultSearchEngine
            )

            searchEngineGroup.addView(searchEngineView, layoutParams)
        }
    }

    private fun makeButtonFromSearchEngine(
        engine: SearchEngine,
        layoutInflater: LayoutInflater,
        res: Resources,
        allowDeletion: Boolean,
        isSelected: Boolean
    ): View {
        val isCustomSearchEngine = engine.type == SearchEngine.Type.CUSTOM

        val wrapper = layoutInflater.inflate(itemResId, null) as LinearLayout
        wrapper.setOnClickListener { wrapper.radio_button.isChecked = true }
        wrapper.radio_button.tag = engine.id
        wrapper.radio_button.isChecked = isSelected
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
                        is SearchEngineMenu.Item.Edit -> editCustomSearchEngine(wrapper, engine)
                        is SearchEngineMenu.Item.Delete -> deleteSearchEngine(
                            context,
                            engine
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
        val searchEngineId = buttonView.tag.toString()
        val engine = requireNotNull(
            context.components.core.store.state.search.searchEngines.find { searchEngine ->
                searchEngine.id == searchEngineId
            }
        )

        context.components.useCases.searchUseCases.selectSearchEngine(engine)
    }

    private fun editCustomSearchEngine(view: View, engine: SearchEngine) {
        val directions = SearchEngineFragmentDirections
            .actionSearchEngineFragmentToEditCustomSearchEngineFragment(engine.id)

        Navigation.findNavController(view).navigate(directions)
    }

    private fun deleteSearchEngine(
        context: Context,
        engine: SearchEngine
    ) {
        context.components.useCases.searchUseCases.removeSearchEngine(engine)

        MainScope().allowUndo(
            view = context.getRootView()!!,
            message = context
                .getString(R.string.search_delete_search_engine_success_message, engine.name),
            undoActionTitle = context.getString(R.string.snackbar_deleted_undo),
            onCancel = {
                context.components.useCases.searchUseCases.addSearchEngine(engine)
            },
            operation = {}
        )
    }
}
