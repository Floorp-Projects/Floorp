/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard.source.kinto

import java.net.URL

/**
 * Represents an http client, used to
 * make it easy to swap implementations
 * as needed
 */
interface HttpClient {
    /**
     * Performs a GET request to the specified URL, supplying
     * the provided headers
     *
     * @param url destination url
     * @param headers headers to submit with the request
     * @return HTTP response
     */
    fun get(url: URL, headers: Map<String, String>? = null): String
}
