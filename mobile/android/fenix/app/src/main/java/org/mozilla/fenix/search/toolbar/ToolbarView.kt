/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.toolbar

import android.content.Context
import android.graphics.Bitmap
import android.graphics.drawable.BitmapDrawable
import androidx.appcompat.content.res.AppCompatResources
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.content.ContextCompat
import mozilla.components.browser.domains.autocomplete.ShippedDomainsProvider
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.feature.toolbar.ToolbarAutocompleteFeature
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.ktx.android.util.dpToPx
import mozilla.components.support.ktx.android.view.hideKeyboard
import org.mozilla.fenix.R
import org.mozilla.fenix.search.SearchFragmentState
import org.mozilla.fenix.theme.ThemeManager

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
    private val context: Context,
    private val interactor: ToolbarInteractor,
    private val historyStorage: HistoryStorage?,
    private val isPrivate: Boolean,
    val view: BrowserToolbar,
    engine: Engine
) {

    private var isInitialized = false
    private var hasBeenCanceled = false

    init {
        view.apply {
            editMode()

            elevation = TOOLBAR_ELEVATION_IN_DP.dpToPx(resources.displayMetrics).toFloat()

            setOnUrlCommitListener {
                // We're hiding the keyboard as early as possible to prevent the engine view
                // from resizing in case the BrowserFragment is being displayed before the
                // keyboard is gone: https://github.com/mozilla-mobile/fenix/issues/8399
                hideKeyboard()
                interactor.onUrlCommitted(it)
                false
            }

            background =
                AppCompatResources.getDrawable(
                    context, ThemeManager.resolveAttribute(R.attr.foundation, context)
                )

            layoutParams.height = CoordinatorLayout.LayoutParams.MATCH_PARENT

            edit.hint = context.getString(R.string.search_hint)

            edit.colors = edit.colors.copy(
                text = context.getColorFromAttr(R.attr.primaryText),
                hint = context.getColorFromAttr(R.attr.secondaryText),
                suggestionBackground = ContextCompat.getColor(
                    context,
                    R.color.suggestion_highlight_color
                ),
                clear = context.getColorFromAttr(R.attr.primaryText)
            )

            edit.setUrlBackground(
                AppCompatResources.getDrawable(context, R.drawable.search_url_background))

            private = isPrivate

            setOnEditListener(object : mozilla.components.concept.toolbar.Toolbar.OnEditListener {
                override fun onCancelEditing(): Boolean {
                    // For some reason, this can be triggered twice on one back press. This only leads to
                    // navigateUp, so let's make sure we only call it once
                    if (!hasBeenCanceled) interactor.onEditingCanceled()
                    hasBeenCanceled = true
                    // We need to return false to not show display mode
                    return false
                }
                override fun onTextChanged(text: String) {
                    url = text
                    this@ToolbarView.interactor.onTextChanged(text)
                }
            })
        }

        val engineForSpeculativeConnects = if (!isPrivate) engine else null
        ToolbarAutocompleteFeature(
            view,
            engineForSpeculativeConnects
        ).apply {
            addDomainProvider(ShippedDomainsProvider().also { it.initialize(view.context) })
            historyStorage?.also(::addHistoryStorageProvider)
        }
    }

    fun update(searchState: SearchFragmentState) {
        if (!isInitialized) {
            view.url = searchState.pastedText ?: searchState.query

            /* Only set the search terms if pasted text is null so that the search term doesn't
            overwrite pastedText when view enters `editMode` */
            if (searchState.pastedText.isNullOrEmpty()) {
                view.setSearchTerms(searchState.session?.searchTerms.orEmpty())
            }

            // We must trigger an onTextChanged so when search terms are set when transitioning to `editMode`
            // we have the most up to date text
            interactor.onTextChanged(view.url.toString())

            view.editMode()
            isInitialized = true
        }

        val iconSize = context.resources.getDimensionPixelSize(R.dimen.preference_icon_drawable_size)

        val scaledIcon = Bitmap.createScaledBitmap(
            searchState.searchEngineSource.searchEngine.icon,
            iconSize,
            iconSize,
            true)

        val icon = BitmapDrawable(context.resources, scaledIcon)

        view.edit.setIcon(icon, searchState.searchEngineSource.searchEngine.name)
    }

    companion object {
        private const val TOOLBAR_ELEVATION_IN_DP = 16
    }
}
