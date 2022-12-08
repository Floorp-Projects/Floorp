/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.toolbar

import android.content.Context
import android.graphics.Bitmap
import android.graphics.drawable.BitmapDrawable
import androidx.annotation.VisibleForTesting
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.content.ContextCompat
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.ktx.android.content.res.resolveAttribute
import mozilla.components.support.ktx.android.view.hideKeyboard
import org.mozilla.fenix.R
import org.mozilla.fenix.components.Core
import org.mozilla.fenix.search.SearchFragmentState
import org.mozilla.fenix.utils.Settings

/**
 * Interface for the Toolbar Interactor. This interface is implemented by objects that want
 * to respond to user interaction on the [ToolbarView].
 */
interface ToolbarInteractor : SearchSelectorInteractor {

    /**
     * Called when a user hits the return key while [ToolbarView] has focus.
     *
     * @param url The text inside the [ToolbarView] when committed.
     * @param fromHomeScreen True if the toolbar has been opened from home screen.
     */
    fun onUrlCommitted(url: String, fromHomeScreen: Boolean = false)

    /**
     * Called when a user removes focus from the [ToolbarView]
     */
    fun onEditingCanceled()

    /**
     * Called whenever the text inside the [ToolbarView] changes
     *
     * @param text The current text displayed by [ToolbarView].
     */
    fun onTextChanged(text: String)
}

/**
 * View that contains and configures the BrowserToolbar to only be used in its editing mode.
 */
@Suppress("LongParameterList")
class ToolbarView(
    private val context: Context,
    private val settings: Settings,
    private val interactor: ToolbarInteractor,
    private val isPrivate: Boolean,
    val view: BrowserToolbar,
    fromHomeFragment: Boolean,
) {

    @VisibleForTesting
    internal var isInitialized = false

    init {
        view.apply {
            editMode()

            setOnUrlCommitListener {
                // We're hiding the keyboard as early as possible to prevent the engine view
                // from resizing in case the BrowserFragment is being displayed before the
                // keyboard is gone: https://github.com/mozilla-mobile/fenix/issues/8399
                hideKeyboard()
                interactor.onUrlCommitted(it, fromHomeFragment)
                false
            }

            background = AppCompatResources.getDrawable(
                context,
                context.theme.resolveAttribute(R.attr.layer1),
            )

            edit.hint = context.getString(R.string.search_hint)

            edit.colors = edit.colors.copy(
                text = context.getColorFromAttr(R.attr.textPrimary),
                hint = context.getColorFromAttr(R.attr.textSecondary),
                suggestionBackground = ContextCompat.getColor(
                    context,
                    R.color.suggestion_highlight_color,
                ),
                clear = context.getColorFromAttr(R.attr.textPrimary),
            )

            edit.setUrlBackground(
                AppCompatResources.getDrawable(context, R.drawable.search_url_background),
            )

            private = isPrivate

            setOnEditListener(
                object : mozilla.components.concept.toolbar.Toolbar.OnEditListener {
                    override fun onCancelEditing(): Boolean {
                        interactor.onEditingCanceled()
                        // We need to return false to not show display mode
                        return false
                    }

                    override fun onTextChanged(text: String) {
                        url = text
                        interactor.onTextChanged(text)
                    }
                },
            )
        }
    }

    fun update(searchState: SearchFragmentState) {
        if (!isInitialized) {
            view.url = searchState.pastedText ?: searchState.query

            /* Only set the search terms if pasted text is null so that the search term doesn't
            overwrite pastedText when view enters `editMode` */
            if (searchState.pastedText.isNullOrEmpty()) {
                // If we're in edit mode, setting the search term will update the toolbar,
                // so we make sure we have the correct term/query to show.
                val termOrQuery = searchState.searchTerms.ifEmpty {
                    searchState.query
                }
                view.setSearchTerms(termOrQuery)
            }

            // We must trigger an onTextChanged so when search terms are set when transitioning to `editMode`
            // we have the most up to date text
            interactor.onTextChanged(view.url.toString())

            // If search terms are displayed, move the cursor to the end instead of selecting all text.
            if (settings.showUnifiedSearchFeature && searchState.searchTerms.isNotBlank()) {
                view.editMode(cursorPlacement = Toolbar.CursorPlacement.END)
            } else {
                view.editMode()
            }
            isInitialized = true
        }

        val searchEngine = searchState.searchEngineSource.searchEngine

        view.edit.hint = when (searchEngine?.type) {
            SearchEngine.Type.APPLICATION ->
                when (searchEngine.id) {
                    Core.HISTORY_SEARCH_ENGINE_ID -> context.getString(R.string.history_search_hint)
                    Core.BOOKMARKS_SEARCH_ENGINE_ID -> context.getString(R.string.bookmark_search_hint)
                    Core.TABS_SEARCH_ENGINE_ID -> context.getString(R.string.tab_search_hint)
                    else -> context.getString(R.string.application_search_hint)
                }
            SearchEngine.Type.BUNDLED -> {
                if (!searchEngine.isGeneral) {
                    context.getString(R.string.application_search_hint)
                } else {
                    context.getString(R.string.search_hint)
                }
            }
            else ->
                context.getString(R.string.search_hint)
        }

        if (!settings.showUnifiedSearchFeature && searchEngine != null) {
            val iconSize =
                context.resources.getDimensionPixelSize(R.dimen.preference_icon_drawable_size)

            val scaledIcon = Bitmap.createScaledBitmap(
                searchEngine.icon,
                iconSize,
                iconSize,
                true,
            )

            val icon = BitmapDrawable(context.resources, scaledIcon)

            view.edit.setIcon(icon, searchEngine.name)
        }
    }
}
