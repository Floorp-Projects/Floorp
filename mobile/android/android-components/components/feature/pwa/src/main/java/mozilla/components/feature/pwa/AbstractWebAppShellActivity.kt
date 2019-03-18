/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa

import android.os.Bundle
import android.support.annotation.VisibleForTesting
import android.support.v7.app.AppCompatActivity
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.manifest.WebAppManifest
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.pwa.ext.applyOrientation
import mozilla.components.support.ktx.android.view.enterToImmersiveMode

/**
 * Activity for "standalone" and "fullscreen" web applications.
 */
abstract class AbstractWebAppShellActivity : AppCompatActivity() {
    abstract val engine: Engine
    abstract val sessionManager: SessionManager

    lateinit var session: Session
    lateinit var engineView: EngineView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // We do not "install" web apps yet. So there's no place we can load a manifest from yet.
        // https://github.com/mozilla-mobile/android-components/issues/2382
        val manifest = createTestManifest()

        applyConfiguration(manifest)
        renderSession(manifest)
    }

    override fun onDestroy() {
        super.onDestroy()

        sessionManager
            .getOrCreateEngineSession(session)
            .close()
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun applyConfiguration(manifest: WebAppManifest) {
        if (manifest.display == WebAppManifest.DisplayMode.FULLSCREEN) {
            enterToImmersiveMode()
        }

        applyOrientation(manifest)
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun renderSession(manifest: WebAppManifest) {
        setContentView(engine
            .createView(this)
            .also { engineView = it }
            .asView())

        session = Session(manifest.startUrl)
        engineView.render(sessionManager.getOrCreateEngineSession(session))
    }

    companion object {
        const val INTENT_ACTION = "mozilla.components.feature.pwa.SHELL"
    }
}
