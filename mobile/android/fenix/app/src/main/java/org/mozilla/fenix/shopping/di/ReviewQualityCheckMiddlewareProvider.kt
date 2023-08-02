/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.di

import kotlinx.coroutines.CoroutineScope
import org.mozilla.fenix.shopping.middleware.ReviewQualityCheckPreferencesImpl
import org.mozilla.fenix.shopping.middleware.ReviewQualityCheckPreferencesMiddleware
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
     * @param scope The [CoroutineScope] to use for launching coroutines.
     */
    fun provideMiddleware(
        settings: Settings,
        scope: CoroutineScope,
    ): List<ReviewQualityCheckMiddleware> =
        listOf(providePreferencesMiddleware(settings, scope))

    private fun providePreferencesMiddleware(
        settings: Settings,
        scope: CoroutineScope,
    ) = ReviewQualityCheckPreferencesMiddleware(
        reviewQualityCheckPreferences = ReviewQualityCheckPreferencesImpl(
            settings,
        ),
        scope = scope,
    )
}
