/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.net

typealias HeadersList = List<Pair<String, String>>

/**
 * The interface defining how to send pings.
 */
interface PingUploader {
    /**
     * Synchronously upload a ping to a server.
     *
     * @param url the URL path to upload the data to
     * @param data the serialized text data to send
     * @param headers a [HeadersList] containing String to String [Pair] with
     *        the first entry being the header name and the second its value.
     *
     * @return true if the ping was correctly dealt with (sent successfully
     *         or faced an unrecoverable error), false if there was a recoverable
     *         error callers can deal with.
     */
    fun upload(url: String, data: String, headers: HeadersList): Boolean
}
