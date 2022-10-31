/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.webextension

import android.graphics.Bitmap
import android.net.Uri
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.Settings
import org.json.JSONObject

/**
 * Represents a browser extension based on the WebExtension API:
 * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions
 *
 * @property id the unique ID of this extension.
 * @property url the url pointing to a resources path for locating the extension
 * within the APK file e.g. resource://android/assets/extensions/my_web_ext.
 * @property supportActions whether or not browser and page actions are handled when
 * received from the web extension
 */
abstract class WebExtension(
    val id: String,
    val url: String,
    val supportActions: Boolean,
) {
    /**
     * Registers a [MessageHandler] for message events from background scripts.
     *
     * @param name the name of the native "application". This can either be the
     * name of an application, web extension or a specific feature in case
     * the web extension opens multiple [Port]s. There can only be one handler
     * with this name per extension and the same name has to be used in
     * JavaScript when calling `browser.runtime.connectNative` or
     * `browser.runtime.sendNativeMessage`. Note that name must match
     * /^\w+(\.\w+)*$/).
     * @param messageHandler the message handler to be notified of messaging
     * events e.g. a port was connected or a message received.
     */
    abstract fun registerBackgroundMessageHandler(name: String, messageHandler: MessageHandler)

    /**
     * Registers a [MessageHandler] for message events from content scripts.
     *
     * @param session the session to be observed / attach the message handler to.
     * @param name the name of the native "application". This can either be the
     * name of an application, web extension or a specific feature in case
     * the web extension opens multiple [Port]s. There can only be one handler
     * with this name per extension and session, and the same name has to be
     * used in JavaScript when calling `browser.runtime.connectNative` or
     * `browser.runtime.sendNativeMessage`. Note that name must match
     * /^\w+(\.\w+)*$/).
     * @param messageHandler the message handler to be notified of messaging
     * events e.g. a port was connected or a message received.
     */
    abstract fun registerContentMessageHandler(session: EngineSession, name: String, messageHandler: MessageHandler)

    /**
     * Checks whether there is an existing content message handler for the provided
     * session and "application" name.
     *
     * @param session the session the message handler was registered for.
     * @param name the "application" name the message handler was registered for.
     * @return true if a content message handler is active, otherwise false.
     */
    abstract fun hasContentMessageHandler(session: EngineSession, name: String): Boolean

    /**
     * Returns a connected port with the given name and for the provided
     * [EngineSession], if one exists.
     *
     * @param name the name as provided to connectNative.
     * @param session (optional) session to check for, null if port is from a
     * background script.
     * @return a matching port, or null if none is connected.
     */
    abstract fun getConnectedPort(name: String, session: EngineSession? = null): Port?

    /**
     * Disconnect a [Port] of the provided [EngineSession]. This method has
     * no effect if there's no connected port with the given name.
     *
     * @param name the name as provided to connectNative, see
     * [registerContentMessageHandler] and [registerBackgroundMessageHandler].
     * @param session (options) session for which ports should disconnected,
     * null if port is from a background script.
     */
    abstract fun disconnectPort(name: String, session: EngineSession? = null)

    /**
     * Registers an [ActionHandler] for this web extension. The handler will
     * be invoked whenever browser and page action defaults change. To listen
     * for session-specific overrides see registerActionHandler(
     * EngineSession, ActionHandler).
     *
     * @param actionHandler the [ActionHandler] to be invoked when a browser or
     * page action is received.
     */
    abstract fun registerActionHandler(actionHandler: ActionHandler)

    /**
     * Registers an [ActionHandler] for the provided [EngineSession]. The handler
     * will be invoked whenever browser and page action overrides are received
     * for the provided session.
     *
     * @param session the [EngineSession] the handler should be registered for.
     * @param actionHandler the [ActionHandler] to be invoked when a
     * session-specific browser or page action is received.
     */
    abstract fun registerActionHandler(session: EngineSession, actionHandler: ActionHandler)

    /**
     * Checks whether there is an existing action handler for the provided
     * session.
     *
     * @param session the session the action handler was registered for.
     * @return true if an action handler is registered, otherwise false.
     */
    abstract fun hasActionHandler(session: EngineSession): Boolean

    /**
     * Registers a [TabHandler] for this web extension. This handler will
     * be invoked whenever a web extension wants to open a new tab. To listen
     * for session-specific events (such as [TabHandler.onCloseTab]) use
     * registerTabHandler(EngineSession, TabHandler) instead.
     *
     * @param tabHandler the [TabHandler] to be invoked when the web extension
     * wants to open a new tab.
     * @param defaultSettings used to pass default tab settings to any tabs opened by
     * a web extension.
     */
    abstract fun registerTabHandler(tabHandler: TabHandler, defaultSettings: Settings?)

    /**
     * Registers a [TabHandler] for the provided [EngineSession]. The handler
     * will be invoked whenever an existing tab should be closed or updated.
     *
     * @param tabHandler the [TabHandler] to be invoked when the web extension
     * wants to update or close an existing tab.
     */
    abstract fun registerTabHandler(session: EngineSession, tabHandler: TabHandler)

    /**
     * Checks whether there is an existing tab handler for the provided
     * session.
     *
     * @param session the session the tab handler was registered for.
     * @return true if an tab handler is registered, otherwise false.
     */
    abstract fun hasTabHandler(session: EngineSession): Boolean

    /**
     * Returns additional information about this extension.
     *
     * @return extension [Metadata], or null if the extension isn't
     * installed and there is no meta data available.
     */
    abstract fun getMetadata(): Metadata?

    /**
     * Checks whether or not this extension is built-in (packaged with the
     * APK file) or coming from an external source.
     */
    open fun isBuiltIn(): Boolean = Uri.parse(url).scheme == "resource"

    /**
     * Checks whether or not this extension is enabled.
     */
    abstract fun isEnabled(): Boolean

    /**
     * Checks whether or not this extension is allowed in private browsing.
     */
    abstract fun isAllowedInPrivateBrowsing(): Boolean

    /**
     * Returns the icon of this extension as specified in the extension's manifest:
     * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/icons
     *
     * @param size the desired size of the icon. The returned icon will be the closest
     * available icon to the provided size.
     */
    abstract suspend fun loadIcon(size: Int): Bitmap?
}

/**
 * A handler for web extension (browser and page) actions.
 *
 * Page action support will be addressed in:
 * https://github.com/mozilla-mobile/android-components/issues/4470
 */
interface ActionHandler {

    /**
     * Invoked when a browser action is defined or updated.
     *
     * @param extension the extension that defined the browser action.
     * @param session the [EngineSession] if this action is to be updated for a
     * specific session, or null if this is to set a new default value.
     * @param action the browser action as [Action].
     */
    fun onBrowserAction(extension: WebExtension, session: EngineSession?, action: Action) = Unit

    /**
     * Invoked when a page action is defined or updated.
     *
     * @param extension the extension that defined the browser action.
     * @param session the [EngineSession] if this action is to be updated for a
     * specific session, or null if this is to set a new default value.
     * @param action the [Action]
     */
    fun onPageAction(extension: WebExtension, session: EngineSession?, action: Action) = Unit

    /**
     * Invoked when a browser or page action wants to toggle a popup view.
     *
     * @param extension the extension that defined the browser or page action.
     * @param action the action as [Action].
     * @return the [EngineSession] that was used for displaying the popup,
     * or null if the popup was closed.
     */
    fun onToggleActionPopup(extension: WebExtension, action: Action): EngineSession? = null
}

/**
 * A handler for all messaging related events, usable for both content and
 * background scripts.
 *
 * [Port]s are exposed to consumers (higher level components) because
 * how ports are used, how many there are and how messages map to it
 * is feature-specific and depends on the design of the web extension.
 * Therefore it makes most sense to let the extensions (higher-level
 * features) deal with the management of ports.
 */
interface MessageHandler {

    /**
     * Invoked when a [Port] was connected as a result of a
     * `browser.runtime.connectNative` call in JavaScript.
     *
     * @param port the connected port.
     */
    fun onPortConnected(port: Port) = Unit

    /**
     * Invoked when a [Port] was disconnected or the corresponding session was
     * destroyed.
     *
     * @param port the disconnected port.
     */
    fun onPortDisconnected(port: Port) = Unit

    /**
     * Invoked when a message was received on the provided port.
     *
     * @param message the received message, either be a primitive type
     * or a org.json.JSONObject.
     * @param port the port the message was received on.
     */
    fun onPortMessage(message: Any, port: Port) = Unit

    /**
     * Invoked when a message was received as a result of a
     * `browser.runtime.sendNativeMessage` call in JavaScript.
     *
     * @param message the received message, either be a primitive type
     * or a org.json.JSONObject.
     * @param source the session this message originated from if from a content
     * script, otherwise null.
     * @return the response to be sent for this message, either a primitive
     * type or a org.json.JSONObject, null if no response should be sent.
     */
    fun onMessage(message: Any, source: EngineSession?): Any? = Unit
}

/**
 * A handler for all tab related events (triggered by browser.tabs.* methods).
 */
interface TabHandler {

    /**
     * Invoked when a web extension attempts to open a new tab via
     * browser.tabs.create.
     *
     * @param webExtension The [WebExtension] that wants to open the tab.
     * @param engineSession an instance of engine session to open a new tab with.
     * @param active whether or not the new tab should be active/selected.
     * @param url the target url to be loaded in a new tab.
     */
    fun onNewTab(webExtension: WebExtension, engineSession: EngineSession, active: Boolean, url: String) = Unit

    /**
     * Invoked when a web extension attempts to update a tab via
     * browser.tabs.update.
     *
     * @param webExtension The [WebExtension] that wants to update the tab.
     * @param engineSession an instance of engine session to open a new tab with.
     * @param active whether or not the new tab should be active/selected.
     * @param url the (optional) target url to be loaded in a new tab if it has changed.
     * @return true if the tab was updated, otherwise false.
     */
    fun onUpdateTab(webExtension: WebExtension, engineSession: EngineSession, active: Boolean, url: String?) = false

    /**
     * Invoked when a web extension attempts to close a tab via
     * browser.tabs.remove.
     *
     * @param webExtension The [WebExtension] that wants to remove the tab.
     * @param engineSession then engine session of the tab to be closed.
     * @return true if the tab was closed, otherwise false.
     */
    fun onCloseTab(webExtension: WebExtension, engineSession: EngineSession) = false
}

/**
 * Represents a port for exchanging messages:
 * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/runtime/Port
 */
abstract class Port(val engineSession: EngineSession? = null) {

    /**
     * Sends a message to this port.
     *
     * @param message the message to send.
     */
    abstract fun postMessage(message: JSONObject)

    /**
     * Returns the name of this port.
     */
    abstract fun name(): String

    /**
     * Returns the URL of the port sender.
     */
    abstract fun senderUrl(): String

    /**
     * Disconnects this port.
     */
    abstract fun disconnect()
}

/**
 * Provides information about a [WebExtension].
 */
data class Metadata(
    /**
     * Version string:
     * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/version
     */
    val version: String,

    /**
     * Required extension permissions:
     * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/permissions#API_permissions
     */
    val permissions: List<String>,

    /**
     * Required host permissions:
     * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/permissions#Host_permissions
     */
    val hostPermissions: List<String>,

    /**
     * Name of the extension:
     * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/name
     */
    val name: String?,

    /**
     * Description of the extension:
     * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/description
     */
    val description: String?,

    /**
     * Name of the extension developer:
     * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/developer
     */
    val developerName: String?,

    /**
     * Url of the developer:
     * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/developer
     */
    val developerUrl: String?,

    /**
     * Url of extension's homepage:
     * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/homepage_url
     */
    val homePageUrl: String?,

    /**
     * Options page:
     * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/options_ui
     */
    val optionsPageUrl: String?,

    /**
     * Whether or not the options page should be opened in a new tab:
     * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/options_ui#syntax
     */
    val openOptionsPageInTab: Boolean,

    /**
     * Describes the reason (or reasons) why an extension is disabled.
     */
    val disabledFlags: DisabledFlags,

    /**
     * Base URL for pages of this extension. Can be used to determine if a page
     * is from / belongs to this extension.
     */
    val baseUrl: String,

    /**
     * Whether or not this extension is temporary i.e. installed using a debug tool
     * such as web-ext, and won't be retained when the application exits.
     */
    val temporary: Boolean = false,
)

/**
 * Provides additional information about why an extension is being enabled or disabled.
 */
@Suppress("MagicNumber")
enum class EnableSource(val id: Int) {
    /**
     * The extension is enabled or disabled by the user.
     */
    USER(1),

    /**
     * The extension is enabled or disabled by the application based
     * on available support.
     */
    APP_SUPPORT(1 shl 1),
}

/**
 * Flags to check for different reasons why an extension is disabled.
 */
class DisabledFlags internal constructor(val value: Int) {
    companion object {
        const val USER: Int = 1 shl 1
        const val BLOCKLIST: Int = 1 shl 2
        const val APP_SUPPORT: Int = 1 shl 3

        /**
         * Selects a combination of flags.
         *
         * @param flags the flags to select.
         */
        fun select(vararg flags: Int) = DisabledFlags(flags.sum())
    }

    /**
     * Checks if the provided flag is set.
     *
     * @param flag the flag to check.
     */
    fun contains(flag: Int) = (value and flag) != 0
}

/**
 * Returns whether or not the extension is disabled because it is unsupported.
 */
fun WebExtension.isUnsupported(): Boolean {
    val flags = getMetadata()?.disabledFlags
    return flags?.contains(DisabledFlags.APP_SUPPORT) == true
}

/**
 * An unexpected event that occurs when trying to perform an action on the extension like
 * (but not exclusively) installing/uninstalling, removing or updating.
 */
open class WebExtensionException(throwable: Throwable, open val isRecoverable: Boolean = true) : Exception(throwable)
