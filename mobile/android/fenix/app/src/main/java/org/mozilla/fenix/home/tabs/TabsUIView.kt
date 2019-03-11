/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.tabs

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.content.ContextCompat
import androidx.navigation.Navigation
import androidx.recyclerview.widget.DefaultItemAnimator
import androidx.recyclerview.widget.LinearLayoutManager
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import kotlinx.android.synthetic.main.component_tabs.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.home.HomeFragment
import org.mozilla.fenix.home.HomeFragmentDirections
import org.mozilla.fenix.mvi.UIView

class TabsUIView(
    container: ViewGroup,
    actionEmitter: Observer<TabsAction>,
    changesObservable: Observable<TabsChange>,
    private val isPrivate: Boolean
) :
    UIView<TabsState, TabsAction, TabsChange>(container, actionEmitter, changesObservable) {

    override val view: View = LayoutInflater.from(container.context)
        .inflate(R.layout.component_tabs, container, true)

    private val tabsAdapter = TabsAdapter(actionEmitter)

    init {
        view.tabs_list.apply {
            layoutManager = LinearLayoutManager(container.context)
            adapter = tabsAdapter
            itemAnimator = DefaultItemAnimator()
        }
        view.apply {
            add_tab_button.increaseTapArea(HomeFragment.addTabButtonIncreaseDps)
            add_tab_button.setOnClickListener {
                val directions = HomeFragmentDirections.actionHomeFragmentToSearchFragment(null)
                Navigation.findNavController(it).navigate(directions)
            }

            val headerTextResourceId = if (isPrivate) R.string.tabs_header_private_title else R.string.tabs_header_title
            header_text.text = context.getString(headerTextResourceId)
            tabs_overflow_button.increaseTapArea(HomeFragment.overflowButtonIncreaseDps)
            tabs_overflow_button.setOnClickListener {
                actionEmitter.onNext(TabsAction.MenuTapped)
            }

            save_session_button_text.apply {
                val color = ContextCompat.getColor(context, R.color.photonWhite)
                val drawable = ContextCompat.getDrawable(context, R.drawable.ic_archive)
                drawable?.setTint(color)
                this.setCompoundDrawablesWithIntrinsicBounds(drawable, null, null, null)
            }

            delete_session_button.setOnClickListener {
                actionEmitter.onNext(TabsAction.CloseAll(true))
            }

            save_session_button.setOnClickListener {
                actionEmitter.onNext(TabsAction.Archive)
            }
        }
    }

    override fun updateView() = Consumer<TabsState> {
        tabsAdapter.sessions = it.sessions
        val sessionButton = if (isPrivate) view.delete_session_button else view.save_session_button

        (if (it.sessions.isEmpty()) View.GONE else View.VISIBLE).also { visibility ->
            view.tabs_header.visibility = visibility
            sessionButton.visibility = visibility
        }
    }
}
