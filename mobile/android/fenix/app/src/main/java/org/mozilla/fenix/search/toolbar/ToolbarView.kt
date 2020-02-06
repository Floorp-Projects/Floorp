/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.toolbar

import android.graphics.Bitmap
import android.graphics.drawable.BitmapDrawable
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.LayoutRes
import androidx.appcompat.content.res.AppCompatResources
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.content.ContextCompat
import com.google.android.material.appbar.AppBarLayout
import com.google.android.material.appbar.AppBarLayout.LayoutParams.SCROLL_FLAG_ENTER_ALWAYS
import com.google.android.material.appbar.AppBarLayout.LayoutParams.SCROLL_FLAG_EXIT_UNTIL_COLLAPSED
import com.google.android.material.appbar.AppBarLayout.LayoutParams.SCROLL_FLAG_SCROLL
import com.google.android.material.appbar.AppBarLayout.LayoutParams.SCROLL_FLAG_SNAP
import kotlinx.android.extensions.LayoutContainer
import mozilla.components.browser.domains.autocomplete.ShippedDomainsProvider
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.behavior.BrowserToolbarBottomBehavior
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.feature.toolbar.ToolbarAutocompleteFeature
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.ktx.android.util.dpToPx
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.settings
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
    private val container: ViewGroup,
    private val interactor: ToolbarInteractor,
    private val historyStorage: HistoryStorage?,
    private val isPrivate: Boolean
) : LayoutContainer {

    override val containerView: View?
        get() = container

    private val settings = container.context.settings()

    @LayoutRes
    private val toolbarLayout = when {
        settings.shouldUseBottomToolbar -> R.layout.component_bottom_browser_toolbar
        else -> R.layout.component_browser_top_toolbar
    }

    val view: BrowserToolbar = LayoutInflater.from(container.context)
        .inflate(toolbarLayout, container, true)
        .findViewById(R.id.toolbar)

    private var isInitialized = false
    private var hasBeenCanceled = false

    init {
        view.apply {
            editMode()

            setScrollFlagsForTopToolbar()

            elevation = TOOLBAR_ELEVATION_IN_DP.dpToPx(resources.displayMetrics).toFloat()

            setOnUrlCommitListener {
                interactor.onUrlCommitted(it)
                false
            }

            background =
                AppCompatResources.getDrawable(
                    container.context, ThemeManager.resolveAttribute(R.attr.foundation, context)
                )

            layoutParams.height = CoordinatorLayout.LayoutParams.MATCH_PARENT

            edit.hint = context.getString(R.string.search_hint)

            edit.colors = edit.colors.copy(
                text = container.context.getColorFromAttr(R.attr.primaryText),
                hint = container.context.getColorFromAttr(R.attr.secondaryText),
                suggestionBackground = ContextCompat.getColor(
                    container.context,
                    R.color.suggestion_highlight_color
                ),
                clear = container.context.getColorFromAttr(R.attr.primaryText)
            )

            edit.setUrlBackground(
                AppCompatResources.getDrawable(container.context, R.drawable.search_url_background))

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

        ToolbarAutocompleteFeature(view).apply {
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

            view.editMode()
            isInitialized = true
        }

        val iconSize = container.resources.getDimensionPixelSize(R.dimen.preference_icon_drawable_size)

        val scaledIcon = Bitmap.createScaledBitmap(
            searchState.searchEngineSource.searchEngine.icon,
            iconSize,
            iconSize,
            true)

        val icon = BitmapDrawable(container.resources, scaledIcon)

        view.edit.setIcon(icon, searchState.searchEngineSource.searchEngine.name)
    }

    companion object {
        private const val TOOLBAR_ELEVATION_IN_DP = 16
    }
}

/**
 * Dynamically sets scroll flags for the top toolbar when the user does not have a screen reader enabled
 * Note that the bottom toolbar is currently fixed and will never have scroll flags set
 */
fun BrowserToolbar.setScrollFlagsForTopToolbar() {
    // Don't set scroll flags for bottom toolbar
    if (context.settings().shouldUseBottomToolbar) {
        if (FeatureFlags.dynamicBottomToolbar && layoutParams is CoordinatorLayout.LayoutParams) {
            (layoutParams as CoordinatorLayout.LayoutParams).apply {
                behavior = BrowserToolbarBottomBehavior(context, null)
            }
        }

        return
    }

    val params = layoutParams as AppBarLayout.LayoutParams
    params.scrollFlags = when (context.settings().shouldUseFixedTopToolbar) {
        true -> 0
        false -> {
            SCROLL_FLAG_SCROLL or SCROLL_FLAG_ENTER_ALWAYS or SCROLL_FLAG_SNAP or
                SCROLL_FLAG_EXIT_UNTIL_COLLAPSED
        }
    }
}
