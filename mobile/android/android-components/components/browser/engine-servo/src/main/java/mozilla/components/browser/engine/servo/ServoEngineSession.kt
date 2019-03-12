/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.servo

import android.net.Uri
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.concept.engine.Settings
import org.mozilla.servoview.Servo
import org.mozilla.servoview.ServoView
import java.lang.ref.WeakReference

/**
 * Servo-based EngineSession implementation.
 */
@Suppress("TooManyFunctions")
class ServoEngineSession(
    private val defaultSettings: Settings? = null
) : EngineSession() {
    override val settings: Settings
        get() = defaultSettings ?: DefaultSettings()

    private var urlToLoad: String? = null
    private var viewReference: WeakReference<ServoEngineView>? = null
    internal val view: ServoView?
        get() = viewReference?.get()?.servoView

    @Suppress("TooGenericExceptionCaught")
    internal fun attachView(view: ServoEngineView) {
        viewReference = WeakReference(view)

        view.servoView.setClient(client)
        view.servoView.requestFocus()

        urlToLoad?.let {
            view.servoView.loadUri(Uri.parse(it))
        }
    }

    private val client = object : Servo.Client {
        override fun onLoadStarted() {
            notifyObservers {
                onLoadingStateChange(true)
            }
        }

        override fun onRedrawing(redrawing: Boolean) = Unit

        override fun onTitleChanged(title: String) {
            notifyObservers {
                onTitleChange(title)
            }
        }

        override fun onLoadEnded() {
            notifyObservers {
                onLoadingStateChange(false)
            }
        }

        override fun onUrlChanged(url: String) {
            notifyObservers {
                onLocationChange(url)
            }
        }

        override fun onHistoryChanged(canGoBack: Boolean, canGoForward: Boolean) {
            notifyObservers {
                onNavigationStateChange(canGoBack, canGoForward)
            }
        }
    }

    override fun loadUrl(url: String) {
        val view = view
        if (view != null) {
            view.loadUri(Uri.parse(url))
        } else {
            urlToLoad = url
        }
    }

    override fun loadData(data: String, mimeType: String, encoding: String) {
        // not implemented yet
    }

    override fun stopLoading() {
        view?.stop()
    }

    override fun reload() {
        view?.reload()
    }

    override fun goBack() {
        view?.goBack()
    }

    override fun goForward() {
        view?.goForward()
    }

    override fun saveState(): EngineSessionState {
        return ServoEngineSessionState()
    }

    override fun restoreState(state: EngineSessionState) {
        // not implemented yet
    }

    override fun enableTrackingProtection(policy: TrackingProtectionPolicy) {
        // not implemented yet
    }

    override fun disableTrackingProtection() {
        // not implemented yet
    }

    override fun toggleDesktopMode(enable: Boolean, reload: Boolean) {
        // not implemented yet
    }

    override fun clearData() {
        // not implemented yet
    }

    override fun findAll(text: String) {
        // not implemented yet
    }

    override fun findNext(forward: Boolean) {
        // not implemented yet
    }

    override fun clearFindMatches() {
        // not implemented yet
    }

    override fun exitFullScreenMode() {
        // not implemented yet
    }
}
