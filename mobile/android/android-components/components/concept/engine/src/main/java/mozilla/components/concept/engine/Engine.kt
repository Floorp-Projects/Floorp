/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine

import android.content.Context
import android.util.AttributeSet
import mozilla.components.concept.engine.webextension.WebExtension
import org.json.JSONObject
import java.lang.UnsupportedOperationException

/**
 * Entry point for interacting with the engine implementation.
 */
interface Engine {

    /**
     * Creates a new view for rendering web content.
     *
     * @param context an application context
     * @param attrs optional set of attributes
     *
     * @return new newly created [EngineView].
     */
    fun createView(context: Context, attrs: AttributeSet? = null): EngineView

    /**
     * Creates a new engine session.
     *
     * @param private whether or not this session should use private mode.
     *
     * @return the newly created [EngineSession].
     */
    fun createSession(private: Boolean = false): EngineSession

    /**
     * Create a new [EngineSessionState] instance from the serialized JSON representation.
     */
    fun createSessionState(json: JSONObject): EngineSessionState

    /**
     * Returns the name of this engine. The returned string might be used
     * in filenames and must therefore only contain valid filename
     * characters.
     *
     * @return the engine name as specified by concrete implementations.
     */
    fun name(): String

    /**
     * Opens a speculative connection to the host of [url].
     *
     * This is useful if an app thinks it may be making a request to that host in the near future. If no request
     * is made, the connection will be cleaned up after an unspecified.
     *
     * Not all [Engine] implementations may actually implement this.
     */
    fun speculativeConnect(url: String)

    /**
     * Installs the provided extension in this engine.
     *
     * @param ext the [WebExtension] to install.
     * @param onSuccess (optional) callback invoked if the extension was installed successfully.
     * @param onError (optional) callback invoked if there was an error installing the extension.
     * This callback is invoked with an [UnsupportedOperationException] in case the engine doesn't
     * have web extension support.
     */
    fun installWebExtension(
        ext: WebExtension,
        onSuccess: ((WebExtension) -> Unit) = { },
        onError: ((WebExtension, Throwable) -> Unit) = { _, _ -> }
    ): Unit = onError(ext, UnsupportedOperationException("Web extension support is not available in this engine"))

    /**
     * Provides access to the settings of this engine.
     */
    val settings: Settings
}
