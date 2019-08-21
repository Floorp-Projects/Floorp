/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.toolbar

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.content.ContextCompat
import kotlinx.android.extensions.LayoutContainer
import mozilla.components.browser.domains.autocomplete.ShippedDomainsProvider
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.feature.toolbar.ToolbarAutocompleteFeature
import mozilla.components.support.ktx.android.util.dpToPx
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getColorFromAttr
import org.mozilla.fenix.search.SearchState

/**
 * Interface for the Toolbar Interactor. This interface is implemented by objects that want
 * to respond to user interaction on the [BrowserToolbarView]
 */
interface ToolbarInteractor {

    /**
     * Called when a user hits the return key while [BrowserToolbarView] has focus.
     * @param url the text inside the [BrowserToolbarView] when committed
     */
    fun onUrlCommitted(url: String)

    /**
     * Called when a user removes focus from the [BrowserToolbarView]
     */
    fun onEditingCanceled()

    /**
     * Called whenever the text inside the [BrowserToolbarView] changes
     * @param text the current text displayed by [BrowserToolbarView]
     */
    fun onTextChanged(text: String)
}

/**
 * View that contains and configures the BrowserToolbar to only be used in its editing mode.
 */
class ToolbarView(
    private val container: ViewGroup,
    private val interactor: ToolbarInteractor,
    private val historyStorage: HistoryStorage?,
    private val isPrivate: Boolean
) : LayoutContainer {

    override val containerView: View?
        get() = container

    val view: BrowserToolbar = LayoutInflater.from(container.context)
        .inflate(R.layout.component_search, container, true)
        .findViewById(R.id.toolbar)

    private var isInitialized = false

    init {
        view.apply {
            editMode()

            elevation = TOOLBAR_ELEVATION_IN_DP.dpToPx(resources.displayMetrics).toFloat()

            setOnUrlCommitListener {
                interactor.onUrlCommitted(it)
                false
            }

            background = null

            layoutParams.height = CoordinatorLayout.LayoutParams.MATCH_PARENT

            hint = context.getString(R.string.search_hint)

            textColor = container.context.getColorFromAttr(R.attr.primaryText)

            hintColor = container.context.getColorFromAttr(R.attr.secondaryText)

            suggestionBackgroundColor = ContextCompat.getColor(
                container.context,
                R.color.suggestion_highlight_color
            )

            private = isPrivate

            setOnEditListener(object : mozilla.components.concept.toolbar.Toolbar.OnEditListener {
                override fun onCancelEditing(): Boolean {
                    interactor.onEditingCanceled()
                    // We need to return false to not show display mode
                    return false
                }
                override fun onTextChanged(text: String) {
                    url = text
                    this@ToolbarView.interactor.onTextChanged(text)
                }
            })
        }

        ToolbarAutocompleteFeature(view).apply {
            addDomainProvider(ShippedDomainsProvider().also { it.initialize(view.context) })
            historyStorage?.also(::addHistoryStorageProvider)
        }
    }

    fun update(searchState: SearchState) {
        if (!isInitialized) {
            view.url = searchState.query
            view.setSearchTerms(searchState.session?.searchTerms ?: "")
            view.editMode()
            isInitialized = true
        }
    }

    companion object {
        private const val TOOLBAR_ELEVATION_IN_DP = 16
    }
}
