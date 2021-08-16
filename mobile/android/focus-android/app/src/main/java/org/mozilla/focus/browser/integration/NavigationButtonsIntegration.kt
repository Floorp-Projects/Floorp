/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.browser.integration

import android.content.Context
import android.graphics.drawable.Drawable
import android.view.View
import android.widget.ImageButton
import androidx.core.content.ContextCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.selector.findCustomTabOrSelectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged
import org.mozilla.focus.R

class NavigationButtonsIntegration(
    val context: Context,
    val store: BrowserStore,
    val toolbar: BrowserToolbar,
    val sessionUseCases: SessionUseCases,
    val customTabId: String?
) : LifecycleAwareFeature {
    private var scope: CoroutineScope? = null

    init {
        toolbar.addNavigationAction(
            NavigationButton(
                image = ContextCompat.getDrawable(context, R.drawable.ic_back)!!,
                contentDescription = context.getString(R.string.content_description_back),
                isEnabled = { store.state.findCustomTabOrSelectedTab(customTabId)?.content?.canGoBack ?: false },
                listener = { sessionUseCases.goBack(store.state.findCustomTabOrSelectedTab(customTabId)?.id) }
            )
        )

        toolbar.addNavigationAction(
            NavigationButton(
                image = ContextCompat.getDrawable(context, R.drawable.ic_forward)!!,
                contentDescription = context.getString(R.string.content_description_forward),
                isEnabled = { store.state.findCustomTabOrSelectedTab(customTabId)?.content?.canGoForward ?: false },
                listener = { sessionUseCases.goForward(store.state.findCustomTabOrSelectedTab(customTabId)?.id) }
            )
        )

        toolbar.addNavigationAction(
            RefreshStopButton(
                refreshImage = ContextCompat.getDrawable(context, R.drawable.ic_refresh)!!,
                stopImage = ContextCompat.getDrawable(context, R.drawable.ic_stop)!!,
                refreshContentDescription = context.getString(R.string.content_description_reload),
                stopContentDescription = context.getString(R.string.content_description_stop),
                isLoading = { store.state.findCustomTabOrSelectedTab(customTabId)?.content?.loading ?: false },
                listener = {
                    val tab = store.state.findCustomTabOrSelectedTab(customTabId) ?: return@RefreshStopButton
                    if (tab.content.loading) {
                        sessionUseCases.stopLoading(tab.id)
                    } else {
                        sessionUseCases.reload(tab.id)
                    }
                }
            )
        )
    }

    override fun start() {
        scope = store.flowScoped { flow ->
            flow.map { state -> state.findCustomTabOrSelectedTab(customTabId) }
                .ifAnyChanged { tab ->
                    arrayOf(
                        tab?.content?.canGoBack,
                        tab?.content?.canGoForward,
                        tab?.content?.loading
                    )
                }
                .collect { toolbar.invalidateActions() }
        }
    }

    override fun stop() {
        scope?.cancel()
    }
}

private class NavigationButton(
    private val image: Drawable,
    contentDescription: String,
    private val isEnabled: () -> Boolean = { true },
    listener: () -> Unit
) : BrowserToolbar.Button(
    image,
    contentDescription,
    listener = listener
) {
    private var enabled: Boolean = false

    @Suppress("MagicNumber")
    private val disabledImage: Drawable = image
        .constantState!!
        .newDrawable()
        .mutate()
        .apply {
            alpha = 128
        }

    override fun bind(view: View) {
        enabled = isEnabled.invoke()

        val button = view as ImageButton
        button.contentDescription = contentDescription
        button.isEnabled = enabled

        button.setImageDrawable(
            if (enabled) {
                image
            } else {
                disabledImage
            }
        )
    }
}

private class RefreshStopButton(
    private val refreshImage: Drawable,
    private val stopImage: Drawable,
    private val refreshContentDescription: String,
    private val stopContentDescription: String,
    private val isLoading: () -> Boolean = { true },
    listener: () -> Unit
) : BrowserToolbar.Button(
    refreshImage,
    refreshContentDescription,
    listener = listener
) {
    override fun bind(view: View) {
        val loading = isLoading()

        val button = view as ImageButton

        if (loading) {
            button.setImageDrawable(stopImage)
            button.contentDescription = stopContentDescription
        } else {
            button.setImageDrawable(refreshImage)
            button.contentDescription = refreshContentDescription
        }
    }
}
