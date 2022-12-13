/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.engine

import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response

/**
 * A wrapper around [Client] preventing [Request]s without the private flag set.
 */
class ClientWrapper(
    private val actual: Client,
) : Client() {
    override fun fetch(request: Request): Response {
        if (!request.private) {
            throw IllegalStateException("Non-private request")
        }

        return actual.fetch(request)
    }

    @Deprecated("Non-private Client usage should be prevented")
    fun unwrap(): Client {
        return actual
    }
}
