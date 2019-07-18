/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.core.content.ContextCompat
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import mozilla.components.browser.domains.autocomplete.ShippedDomainsProvider
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.support.ktx.android.util.dpToFloat
import mozilla.components.support.ktx.android.util.dpToPx
import org.mozilla.fenix.R
import org.mozilla.fenix.customtabs.CustomTabToolbarMenu
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.mvi.UIView

class ToolbarUIView(
    sessionId: String?,
    isPrivate: Boolean,
    container: ViewGroup,
    actionEmitter: Observer<SearchAction>,
    changesObservable: Observable<SearchChange>
) :
    UIView<SearchState, SearchAction, SearchChange>(container, actionEmitter, changesObservable) {

    val toolbarIntegration: ToolbarIntegration

    override val view: BrowserToolbar = LayoutInflater.from(container.context)
        .inflate(R.layout.component_search, container, true)
        .findViewById(R.id.toolbar)

    private val urlBackground = LayoutInflater.from(container.context)
        .inflate(R.layout.layout_url_background, container, false)

    init {
        val sessionManager = view.context.components.core.sessionManager
        val session = sessionId?.let { sessionManager.findSessionById(it) }
            ?: sessionManager.selectedSession
        val isCustomTabSession = session?.isCustomTabSession() == true

        view.apply {
            elevation = TOOLBAR_ELEVATION.dpToFloat(resources.displayMetrics)

            onUrlClicked = {
                actionEmitter.onNext(SearchAction.ToolbarClicked)
                false
            }

            browserActionMargin = browserActionMarginDp.dpToPx(resources.displayMetrics)

            urlBoxView = if (isCustomTabSession) null else urlBackground
            progressBarGravity = if (isCustomTabSession) PROGRESS_BOTTOM else PROGRESS_TOP

            textColor = ContextCompat.getColor(context, R.color.photonGrey30)

            hint = context.getString(R.string.search_hint)
        }

        with(view.context) {
            val menuToolbar = if (isCustomTabSession) {
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

    override fun updateView() = Consumer<SearchState> {}

    companion object {
        private const val TOOLBAR_ELEVATION = 16
        private const val PROGRESS_BOTTOM = 0
        private const val PROGRESS_TOP = 1
        const val browserActionMarginDp = 8
    }
}
