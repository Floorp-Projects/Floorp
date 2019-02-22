/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons

import android.content.Context
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.async
import mozilla.components.browser.icons.generator.DefaultIconGenerator
import mozilla.components.browser.icons.generator.IconGenerator
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.support.base.log.logger.Logger
import java.util.concurrent.Executors

// Number of worker threads we are using internally.
private const val THREADS = 3

/**
 * Entry point for loading icons for websites.
 */
class BrowserIcons(
    private val context: Context,
    private val generator: IconGenerator = DefaultIconGenerator(context),
    jobDispatcher: CoroutineDispatcher = Executors.newFixedThreadPool(THREADS).asCoroutineDispatcher()
) {
    private val scope = CoroutineScope(jobDispatcher)

    /**
     * Asynchronously loads an [Icon] for the given [IconRequest].
     */
    fun loadIcon(request: IconRequest): Deferred<Icon> = scope.async {
        // For now we only generate an icon.
        generator.generate(context, request)
    }

    /**
     * Installs the "icons" extension in the engine in order to dynamically load icons for loaded websites.
     */
    fun install(engine: Engine) {
        engine.installWebExtension(
            WebExtension(
                id = "browser-icons",
                url = "resource://android/assets/extensions/browser-icons/"),
            onSuccess = {
                Logger.debug("Installed browser-icons extension")
            },
            onError = { _, throwable ->
                Logger.error("Could not install browser-icons extension", throwable)
            })
    }
}
