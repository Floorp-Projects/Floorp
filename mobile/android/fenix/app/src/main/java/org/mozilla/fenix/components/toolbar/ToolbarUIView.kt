/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.graphics.drawable.BitmapDrawable
import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.ImageView
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import mozilla.components.browser.domains.autocomplete.ShippedDomainsProvider
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.support.ktx.android.content.res.pxToDp
import org.jetbrains.anko.backgroundDrawable
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.mvi.UIView

class ToolbarUIView(
    sessionId: String?,
    isPrivate: Boolean,
    container: ViewGroup,
    actionEmitter: Observer<SearchAction>,
    changesObservable: Observable<SearchChange>,
    private val engineIconView: ImageView? = null
) :
    UIView<SearchState, SearchAction, SearchChange>(container, actionEmitter, changesObservable) {

    val toolbarIntegration: ToolbarIntegration
    var state: SearchState? = null
        private set

    override val view: BrowserToolbar = LayoutInflater.from(container.context)
        .inflate(R.layout.component_search, container, true)
        .findViewById(R.id.toolbar)

    private val urlBackground = LayoutInflater.from(container.context)
        .inflate(R.layout.layout_url_background, container, false)

    init {
        view.apply {
            setOnUrlCommitListener {
                actionEmitter.onNext(SearchAction.UrlCommitted(it, sessionId, state?.engine))
            false
            }
            onUrlClicked = {
                actionEmitter.onNext(SearchAction.ToolbarTapped)
                false
            }

            browserActionMargin = resources.pxToDp(browserActionMarginDp)
            urlBoxView = urlBackground

            hint = context.getString(R.string.search_hint)

            setOnEditListener(object : mozilla.components.concept.toolbar.Toolbar.OnEditListener {
                override fun onCancelEditing(): Boolean {
                    actionEmitter.onNext(SearchAction.EditingCanceled)
                    return false
                }
                override fun onTextChanged(text: String) {
                    url = text
                    actionEmitter.onNext(SearchAction.TextChanged(text))
                }
            })
        }

        with(view.context) {
            val session = sessionId?.let { components.core.sessionManager.findSessionById(sessionId) }
                ?: components.core.sessionManager.selectedSession

            toolbarIntegration = ToolbarIntegration(
                this,
                view,
                ToolbarMenu(this,
                    sessionId = sessionId,
                    requestDesktopStateProvider = { session?.desktopMode ?: false },
                    onItemTapped = { actionEmitter.onNext(SearchAction.ToolbarMenuItemTapped(it)) }
                ),
                ShippedDomainsProvider().also { it.initialize(this) },
                components.core.historyStorage,
                components.core.sessionManager,
                sessionId,
                isPrivate
            )
        }
    }

    override fun updateView() = Consumer<SearchState> {
        if (shouldUpdateEngineIcon(it)) {
            updateEngineIcon(it)
        }

        if (shouldClearSearchURL(it)) {
            clearSearchURL()
        }

        if (shouldUpdateEditingState(it)) {
            updateEditingState(it)
        }

        state = it
    }

    private fun shouldUpdateEngineIcon(newState: SearchState): Boolean {
        return newState.isEditing && (engineDidChange(newState) || state == null)
    }

    private fun updateEngineIcon(newState: SearchState) {
        with(view.context) {
            val defaultEngineIcon = components.search.searchEngineManager.defaultSearchEngine?.icon
            val searchIcon = newState.engine?.icon ?: defaultEngineIcon
            val draw = BitmapDrawable(searchIcon)
            val iconSize =
                containerView?.context!!.resources.getDimension(R.dimen.preference_icon_drawable_size).toInt()
            draw.setBounds(0, 0, iconSize, iconSize)
            engineIconView?.backgroundDrawable = draw
        }
    }

    private fun shouldClearSearchURL(newState: SearchState): Boolean {
        return newState.engine != state?.engine && view.url == newState.query
    }

    private fun clearSearchURL() {
        view.url = ""
        view.editMode()
    }

    private fun shouldUpdateEditingState(newState: SearchState): Boolean {
        return !engineDidChange(newState)
    }

    private fun updateEditingState(newState: SearchState) {
        if (newState.isEditing) {
            view.url = newState.query
            view.editMode()
        } else {
            view.displayMode()
        }
    }

    private fun engineDidChange(newState: SearchState): Boolean {
        return newState.engine != state?.engine
    }

    companion object {
        const val browserActionMarginDp = 8
    }
}
