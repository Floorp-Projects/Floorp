/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.di

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import mozilla.components.browser.state.store.BrowserStore
import org.mozilla.fenix.shopping.middleware.NetworkCheckerImpl
import org.mozilla.fenix.shopping.middleware.ReviewQualityCheckNavigationMiddleware
import org.mozilla.fenix.shopping.middleware.ReviewQualityCheckNetworkMiddleware
import org.mozilla.fenix.shopping.middleware.ReviewQualityCheckPreferencesImpl
import org.mozilla.fenix.shopping.middleware.ReviewQualityCheckPreferencesMiddleware
import org.mozilla.fenix.shopping.middleware.ReviewQualityCheckServiceImpl
import org.mozilla.fenix.shopping.store.ReviewQualityCheckMiddleware
import org.mozilla.fenix.utils.Settings

/**
 * Provides middleware for review quality check store.
 */
object ReviewQualityCheckMiddlewareProvider {

    /**
     * Provides middlewares for review quality check feature.
     *
     * @param settings The [Settings] instance to use.
     * @param browserStore The [BrowserStore] instance to access state.
     * @param context The [Context] instance to use.
     * @param openLink Opens a link. The callback is invoked with the URL [String] parameter and
     * whether or not it should open in a new or the currently selected tab [Boolean] parameter.
     * @param scope The [CoroutineScope] to use for launching coroutines.
     */
    fun provideMiddleware(
        settings: Settings,
        browserStore: BrowserStore,
        context: Context,
        openLink: (String, Boolean) -> Unit,
        scope: CoroutineScope,
    ): List<ReviewQualityCheckMiddleware> =
        listOf(
            providePreferencesMiddleware(settings, scope),
            provideNetworkMiddleware(browserStore, context, scope),
            provideNavigationMiddleware(openLink, scope),
        )

    private fun providePreferencesMiddleware(
        settings: Settings,
        scope: CoroutineScope,
    ) = ReviewQualityCheckPreferencesMiddleware(
        reviewQualityCheckPreferences = ReviewQualityCheckPreferencesImpl(settings),
        scope = scope,
    )

    private fun provideNetworkMiddleware(
        browserStore: BrowserStore,
        context: Context,
        scope: CoroutineScope,
    ) = ReviewQualityCheckNetworkMiddleware(
        reviewQualityCheckService = ReviewQualityCheckServiceImpl(browserStore),
        networkChecker = NetworkCheckerImpl(context),
        scope = scope,
    )

    private fun provideNavigationMiddleware(
        openLink: (String, Boolean) -> Unit,
        scope: CoroutineScope,
    ) = ReviewQualityCheckNavigationMiddleware(
        openLink = openLink,
        scope = scope,
    )
}
