/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa

import android.os.Bundle
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AppCompatActivity
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.feature.pwa.ext.applyOrientation
import mozilla.components.feature.pwa.ext.asTaskDescription
import mozilla.components.support.ktx.android.view.enterToImmersiveMode
import mozilla.components.support.ktx.android.view.setNavigationBarTheme
import mozilla.components.support.ktx.android.view.setStatusBarTheme

/**
 * Activity for "standalone" and "fullscreen" web applications.
 */
abstract class AbstractWebAppShellActivity : AppCompatActivity(), CoroutineScope by MainScope() {
    abstract val engine: Engine
    abstract val sessionManager: SessionManager

    lateinit var session: Session
    lateinit var engineView: EngineView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val storage = ManifestStorage(this)

        val startUrl = intent.data?.toString() ?: return finish()
        launch {
            val manifest = storage.loadManifest(startUrl)

            applyConfiguration(manifest)
            renderSession(startUrl)

            if (manifest != null) {
                setTaskDescription(manifest.asTaskDescription(null))
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()

        coroutineContext.cancel()
        sessionManager
            .getOrCreateEngineSession(session)
            .close()
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun applyConfiguration(manifest: WebAppManifest?) {
        if (manifest?.display == WebAppManifest.DisplayMode.FULLSCREEN) {
            enterToImmersiveMode()
        }

        applyOrientation(manifest)

        manifest?.themeColor?.let { window.setStatusBarTheme(it) }
        manifest?.backgroundColor?.let { window.setNavigationBarTheme(it) }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun renderSession(startUrl: String) {
        setContentView(engine
            .createView(this)
            .also { engineView = it }
            .asView())

        session = Session(startUrl)
        engineView.render(sessionManager.getOrCreateEngineSession(session))
    }

    companion object {
        const val INTENT_ACTION = "mozilla.components.feature.pwa.SHELL"
    }
}
