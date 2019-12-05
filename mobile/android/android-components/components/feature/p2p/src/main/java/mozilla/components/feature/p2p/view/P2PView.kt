/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.p2p.view

import android.content.Context
import android.view.View

/**
 * An interface for views that can display the peer-to-peer communication toolbar.
 */
@Suppress("TooManyFunctions")
interface P2PView {
    /**
     * Listener to be invoked after the user performs certain actions, such as initiating or
     * accepting a connection.
     */
    var listener: Listener?

    /**
     * This initializes the buttons of a [P2PView] to restore the past button state.
     * It need not be called on the first [P2PView] but should be called after one is destroyed
     * and another inflated. It is up to the implementation how the buttons should be presented
     * (such as whether they should be [View.GONE], [View.INVISIBLE], or just disabled when not
     * being presented. The reset button is enabled whenever the connection buttons are not.
     *
     * @param connectB whether the connection buttons (advertise and discover) should be presented
     * @param sendB whether the send buttons (sendUrl and sendPage) should be presented
     */
    fun initializeButtons(connectB: Boolean, sendB: Boolean)

    /**
     * Optionally displays the status of the connection. The implementation should not take other
     * actions based on the status, since the values of the strings could change.
     *
     * @param status the current status
     */
    fun updateStatus(status: String)

    /**
     * Optionally displays the status of the connection. The implementation should not take other
     * actions based on the id or corresponding string, since the values could change.
     *
     * @param id the string resource id
     */
    fun updateStatus(id: Int)

    /**
     * Handles authentication information about a connection. It is recommended that the view
     * prompt the user as to whether to connect to another device having the specified connection
     * information. It is highly recommended that the [token] be displayed, since it uniquely
     * identifies the connection and cannot be forged.
     *
     * @param neighborId a machine-generated ID uniquely identifying the other device
     * @param neighborName a human-readable name of the other device
     * @param token a short string of characters shared between the two devices
     */
    fun authenticate(neighborId: String, neighborName: String, token: String)

    /**
     * Clears the UI state.
     */
    fun clear()

    /**
     * Casts this [P2PView] interface to an actual Android [View] object.
     */
    fun asView(): View = (this as View)

    /**
     * Indicates that data can be sent to the connected device.
     */
    fun readyToSend()

    /**
     * Indicates that a failure state was entered.
     *
     * @param msg a low-level message describing the failure
     */
    fun failure(msg: String)

    /**
     * Indicates that the requested send operation is complete. The view may optionally display
     * the specified string.
     *
     * @param resid the resource if of a string confirming that the requested send was completed
     */
    fun reportSendComplete(resid: Int)

    /**
     * Handles receipt of a URL from the specified neighbor. For example, the view could prompt the
     * user to accept the URL, upon which [Listener.onSetUrl] would be called. Only an internal
     * error would cause `neighborName` to be null. To be robust, a view should use
     * `neighborName ?: neighborId` as the name of the neighbor.
     *
     * @param neighborId the endpoint ID of the neighbor
     * @param neighborName the name of the neighbor, which will only be null if there was
     *     an internal error in the connection library
     * @param url the URL
     */
    fun receiveUrl(neighborId: String, neighborName: String?, url: String)

    /**
     * Handles receipt of a web page from another device. Only an internal
     * error would cause `neighborName` to be null. To be robust, a view should use
     * `neighborName ?: neighborId` as the name of the neighbor.
     *
     * @param neighborId the endpoint ID of the neighbor
     * @param neighborName the name of the neighbor, which will only be null if there was
     *     an internal error in the connection library
     * @param page a representation of the web page
     */
    fun receivePage(neighborId: String, neighborName: String?, page: String)

    /**
     * An interface enabling the [P2PView] to make requests of a controller.
     */
    interface Listener {
        /**
         * Handles a request to advertise the device's presence and desire to connect.
         */
        fun onAdvertise()

        /**
         * Handles a request to discovery other devices wishing to connect.
         */
        fun onDiscover()

        /**
         * Handles a decision to accept the connection specified by the given token. The value
         * of the token is what was passed to [authenticate].
         *
         * @param token a short string uniquely identifying a connection between two devices
         */
        fun onAccept(token: String)

        /**
         * Handles a decision to reject the connection specified by the given token. The value
         * of the token is what was passed to [authenticate].
         *
         * @param token a short string uniquely identifying a connection between two devices
         */
        fun onReject(token: String)

        /**
         * Handles a request to send the current page's URL to the neighbor.
         */
        fun onSendUrl()

        /**
         * Handles a request to send the current page to the neighbor.
         */
        fun onSendPage()

        /**
         * Handles a request to load the specified URL. This will typically be one sent from a neighbor.
         *
         * @param url the URL
         * @param newTab whether to open the URL in a new tab (true) or the current one (false)
         */
        fun onSetUrl(url: String, newTab: Boolean = false)

        /**
         * Loads the specified data into this tab.
         *
         * @param context the current context
         * @param data the contents of the page
         * @param newTab whether to open the URL in a new tab (true) or the current one (false)
         */
        fun onLoadData(context: Context, data: String, newTab: Boolean = false)

        /**
         * Resets the connection to the neighbor.
         */
        fun onReset()

        /**
         * Handles a request to close the toolbar.
         */
        fun onCloseToolbar()
    }
}
