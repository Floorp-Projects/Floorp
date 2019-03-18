/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa

import android.content.ActivityNotFoundException
import android.content.Intent
import android.os.Bundle
import android.support.annotation.VisibleForTesting
import android.support.annotation.VisibleForTesting.PRIVATE
import android.support.v7.app.AppCompatActivity
import mozilla.components.browser.session.manifest.WebAppManifest
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.kotlin.toUri

/**
 * This activity is launched by Web App shortcuts on the home screen.
 *
 * Based on the Web App Manifest (display) it will decide whether the app is launched in the browser or in a
 * standalone activity.
 */
class WebAppLauncherActivity : AppCompatActivity() {
    private val logger = Logger("WebAppLauncherActivity")

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val manifest = loadManifest()

        routeManifest(manifest)

        finish()
    }

    @VisibleForTesting(otherwise = PRIVATE)
    internal fun routeManifest(manifest: WebAppManifest) {
        when (manifest.display) {
            WebAppManifest.DisplayMode.FULLSCREEN, WebAppManifest.DisplayMode.STANDALONE -> launchWebAppShell()

            // We do not implement "minimal-ui" mode. Following the Web App Manifest spec we fallback to
            // using "browser" in this case.
            WebAppManifest.DisplayMode.MINIMAL_UI -> launchBrowser(manifest)

            WebAppManifest.DisplayMode.BROWSER -> launchBrowser(manifest)
        }
    }

    @VisibleForTesting(otherwise = PRIVATE)
    internal fun launchBrowser(manifest: WebAppManifest) {
        val intent = Intent(Intent.ACTION_VIEW, manifest.startUrl.toUri())
        intent.`package` = packageName

        try {
            startActivity(intent)
        } catch (e: ActivityNotFoundException) {
            logger.error("Package does not handle VIEW intent. Can't launch browser.")
        }
    }

    @VisibleForTesting(otherwise = PRIVATE)
    internal fun launchWebAppShell() {
        val intent = Intent()
        intent.action = AbstractWebAppShellActivity.INTENT_ACTION
        intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK
        intent.`package` = packageName

        try {
            startActivity(intent)
        } catch (e: ActivityNotFoundException) {
            logger.error("Packages does not handle AbstractWebAppShellActivity intent. Can't launch web app.", e)
        }
    }

    @VisibleForTesting(otherwise = PRIVATE)
    internal fun loadManifest(): WebAppManifest {
        // We do not "install" web apps yet. So there's no place we can load a manifest from yet.
        // https://github.com/mozilla-mobile/android-components/issues/2382
        return createTestManifest()
    }
}

/**
 * Just a test manifest we use for testing until we save and load the actual manifests.
 * https://github.com/mozilla-mobile/android-components/issues/2382
 */
internal fun createTestManifest(): WebAppManifest {
    return WebAppManifest(
        name = "Demo",
        startUrl = "https://www.mozilla.org",
        display = WebAppManifest.DisplayMode.FULLSCREEN)
}
