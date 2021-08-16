/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.exceptions

import android.content.Context
import android.content.SharedPreferences
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.ContentPermission.VALUE_ALLOW
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.PERMISSION_TRACKING
import kotlin.collections.forEach as withEach

/**
 * Migrate preference based Focus tracking protection exceptions to gecko based exceptions.
 */
class GeckoExceptionsMigrator(
    private val runtime: GeckoRuntime
) {
    private val logger = Logger("GeckoExceptionsMigrator")
    private val storageController by lazy { runtime.storageController }

    suspend fun start(context: Context) {
        if (!isOver(context)) {
            logger.info("Starting tracking protection exceptions migration")

            val exceptions = fetchExceptions(context)

            withContext(Dispatchers.Main) {
                exceptions.withEach { migrate(it) }
            }

            logger.info("Finished tracking protection exceptions migration")

            // We are not removing the domains yet because GeckoView does not seem to persist them.
            // preferences(context).edit().remove(KEY_DOMAINS).apply()
        }
    }

    /**
     * Fetch the previously added/saved custom domains from preferences.
     *
     * @param context the application context
     * @return list of custom domains
     */
    private fun fetchExceptions(context: Context): List<String> {
        return (
            preferences(context)
                .getString(KEY_DOMAINS, "") ?: ""
            )
            .split(SEPARATOR)
            .filter { it.isNotEmpty() }
    }

    private fun isOver(context: Context) = !preferences(context).contains(KEY_DOMAINS)

    private fun preferences(context: Context): SharedPreferences =
        context.getSharedPreferences(PREFERENCE_NAME, Context.MODE_PRIVATE)

    private fun migrate(uri: String) {
        val privateMode = true
        storageController.setPermission(
            "https://$uri",
            null,
            privateMode,
            PERMISSION_TRACKING,
            VALUE_ALLOW
        )
    }

    companion object {
        private const val PREFERENCE_NAME = "exceptions"
        private const val KEY_DOMAINS = "exceptions_domains"
        private const val SEPARATOR = "@<;>@"
    }
}
