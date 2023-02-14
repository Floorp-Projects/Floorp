/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.toolbar

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.content.ContextCompat
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.ktx.android.content.res.resolveAttribute
import mozilla.components.support.ktx.android.view.hideKeyboard
import org.mozilla.fenix.R
import org.mozilla.fenix.library.bookmarks.BookmarkSearchFragmentState

/**
 * Interface for the Toolbar Interactor. This interface is implemented by objects that want
 * to respond to user interaction on the [ToolbarView]
 */
interface ToolbarInteractor {

    /**
     * Called when a user removes focus from the [ToolbarView]
     */
    fun onEditingCanceled()

    /**
     * Called whenever the text inside the [ToolbarView] changes
     * @param text the current text displayed by [ToolbarView]
     */
    fun onTextChanged(text: String)
}

/**
 * View that contains and configures the BrowserToolbar to only be used in its editing mode.
 */
@Suppress("LongParameterList")
class ToolbarView(
    private val context: Context,
    private val interactor: ToolbarInteractor,
    private val isPrivate: Boolean,
    val view: BrowserToolbar,
) {

    @VisibleForTesting
    internal var isInitialized = false

    init {
        view.apply {
            editMode()

            background = AppCompatResources.getDrawable(
                context,
                context.theme.resolveAttribute(R.attr.layer1),
            )

            edit.hint = context.getString(R.string.bookmark_search)

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

            setOnUrlCommitListener {
                hideKeyboard()

                // We need to return false to not show display mode
                false
            }

            setDefaultIcon()

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

    fun update(state: BookmarkSearchFragmentState) {
        if (!isInitialized) {
            view.url = state.query
            view.setSearchTerms(state.query)

            // We must trigger an onTextChanged so when search terms are set when transitioning to `editMode`
            // we have the most up to date text
            interactor.onTextChanged(view.url.toString())

            view.editMode()
            isInitialized = true
        }
    }

    private fun setDefaultIcon() {
        val bookmarkSearchIcon =
            AppCompatResources.getDrawable(context, R.drawable.ic_bookmarks_menu)

        bookmarkSearchIcon?.let {
            view.edit.setIcon(bookmarkSearchIcon, context.getString(R.string.bookmark_search))
        }
    }
}
