/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.graphics.drawable.BitmapDrawable
import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.ImageView
import androidx.core.content.ContextCompat
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import mozilla.components.browser.domains.autocomplete.ShippedDomainsProvider
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.support.ktx.android.util.dpToFloat
import mozilla.components.support.ktx.android.util.dpToPx
import org.jetbrains.anko.backgroundDrawable
import org.mozilla.fenix.R
import org.mozilla.fenix.customtabs.CustomTabToolbarMenu
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.mvi.UIView

class ToolbarUIView(
    sessionId: String?,
    isPrivate: Boolean,
    startInEditMode: Boolean,
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
        val sessionManager = view.context.components.core.sessionManager
        val session = sessionId?.let { sessionManager.findSessionById(it) }
            ?: sessionManager.selectedSession

        view.apply {
            if (startInEditMode) {
                editMode()
            }

            elevation = TOOLBAR_ELEVATION.dpToFloat(resources.displayMetrics)

            setOnUrlCommitListener {
                actionEmitter.onNext(SearchAction.UrlCommitted(it, sessionId, state?.engine))
            false
            }
            onUrlClicked = {
                actionEmitter.onNext(SearchAction.ToolbarClicked)
                false
            }

            browserActionMargin = browserActionMarginDp.dpToPx(resources.displayMetrics)

            val isCustomTabSession = (session?.isCustomTabSession() == true)

            urlBoxView = if (isCustomTabSession) { null } else urlBackground
            progressBarGravity = if (isCustomTabSession) { PROGRESS_BOTTOM } else PROGRESS_TOP

            textColor = ContextCompat.getColor(context, R.color.photonGrey30)

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
            val isCustom = session?.isCustomTabSession() ?: false

            val menuToolbar = if (isCustom) {
                CustomTabToolbarMenu(
                    this,
                    sessionManager,
                    sessionId,
                    onItemTapped = { actionEmitter.onNext(SearchAction.ToolbarMenuItemTapped(it)) }
                )
            } else {
                DefaultToolbarMenu(this,
                    hasAccountProblem = components.backgroundServices.accountManager.accountNeedsReauth(),
                    requestDesktopStateProvider = { session?.desktopMode ?: false },
                    onItemTapped = { actionEmitter.onNext(SearchAction.ToolbarMenuItemTapped(it)) }
                )
            }

            toolbarIntegration = ToolbarIntegration(
                this,
                view,
                container,
                menuToolbar,
                ShippedDomainsProvider().also { it.initialize(this) },
                components.core.historyStorage,
                components.core.sessionManager,
                sessionId,
                isPrivate
            )
        }
    }

    override fun updateView() = Consumer<SearchState> {
        var newState = it
        if (shouldUpdateEngineIcon(newState)) {
            updateEngineIcon(newState)
        }

        if (shouldClearSearchURL(it)) {
            newState = SearchState("", "", it.isEditing, it.engine, it.focused, it.isQueryUpdated)
        }

        if (newState != it || shouldUpdateEditingState(newState)) {
            updateEditingState(newState)
        }

        if (newState.focused) {
            view.focus()
        } else {
            view.clearFocus()
        }

        state = newState
    }

    private fun shouldUpdateEngineIcon(newState: SearchState): Boolean {
        return newState.isEditing && (engineDidChange(newState) || state == null)
    }

    private fun updateEngineIcon(newState: SearchState) {
        with(view.context) {
            val defaultEngineIcon = components.search.searchEngineManager.defaultSearchEngine?.icon
            val searchIcon = newState.engine?.icon ?: defaultEngineIcon
            val draw = BitmapDrawable(resources, searchIcon)
            val iconSize =
                containerView?.context!!.resources.getDimension(R.dimen.preference_icon_drawable_size).toInt()
            draw.setBounds(0, 0, iconSize, iconSize)
            engineIconView?.backgroundDrawable = draw
        }
    }

    private fun shouldClearSearchURL(newState: SearchState): Boolean {
        with(view.context) {
            val defaultEngine = this
                .components
                .search
                .searchEngineManager
                .defaultSearchEngine

            return (newState.engine != null && newState.engine != defaultEngine) ||
                    (state?.engine != null && state?.engine != defaultEngine)
        }
    }

    private fun shouldUpdateEditingState(newState: SearchState): Boolean {
        return !engineDidChange(newState) && (state?.isEditing != newState.isEditing)
    }

    private fun updateEditingState(newState: SearchState) {
        if (newState.isEditing) {
            view.setSearchTerms(newState.searchTerm)
            view.url = newState.query
            if (!newState.isQueryUpdated) {
                view.editMode()
            }
        } else {
            view.displayMode()
        }
    }

    private fun engineDidChange(newState: SearchState): Boolean {
        return newState.engine != state?.engine
    }

    companion object {
        private const val TOOLBAR_ELEVATION = 16
        private const val PROGRESS_BOTTOM = 0
        private const val PROGRESS_TOP = 1
        const val browserActionMarginDp = 8
    }
}
