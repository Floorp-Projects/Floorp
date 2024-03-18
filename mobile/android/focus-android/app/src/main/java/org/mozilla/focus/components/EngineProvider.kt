/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.components

import android.content.Context
import androidx.datastore.preferences.preferencesDataStore
import mozilla.components.browser.engine.gecko.GeckoEngine
import mozilla.components.browser.engine.gecko.cookiebanners.GeckoCookieBannersStorage
import mozilla.components.browser.engine.gecko.cookiebanners.ReportSiteDomainsRepository
import mozilla.components.browser.engine.gecko.fetch.GeckoViewFetchClient
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.fetch.Client
import mozilla.components.lib.crash.handler.CrashHandlerService
import org.mozilla.focus.utils.AppConstants
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoRuntimeSettings

object EngineProvider {
    private var runtime: GeckoRuntime? = null
    private val Context.dataStore by preferencesDataStore(
        name = ReportSiteDomainsRepository.REPORT_SITE_DOMAINS_REPOSITORY_NAME,
    )

    @Synchronized
    private fun getOrCreateRuntime(context: Context): GeckoRuntime {
        if (runtime == null) {
            val builder = GeckoRuntimeSettings.Builder()

            builder.crashHandler(CrashHandlerService::class.java)
            builder.aboutConfigEnabled(
                AppConstants.isDevOrNightlyBuild || AppConstants.isBetaBuild,
            )

            runtime = GeckoRuntime.create(context, builder.build())
        }

        return runtime!!
    }

    fun createEngine(context: Context, defaultSettings: DefaultSettings): Engine {
        val runtime = getOrCreateRuntime(context)

        return GeckoEngine(context, defaultSettings, runtime)
    }

    fun createCookieBannerStorage(context: Context): GeckoCookieBannersStorage {
        val runtime = getOrCreateRuntime(context)

        return GeckoCookieBannersStorage(runtime, ReportSiteDomainsRepository(context.dataStore))
    }

    fun createClient(context: Context): Client {
        val runtime = getOrCreateRuntime(context)
        return GeckoViewFetchClient(context, runtime)
    }
}
