/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.rusthttp

import mozilla.components.concept.fetch.Client
import mozilla.appservices.httpconfig.RustHttpConfig as AppSvcHttpConfig

/**
 * An object allowing configuring the HTTP client used by Rust code.
 */
object RustHttpConfig {

    /**
     * Set the HTTP client to be used by all Rust code.
     *
     * The `Lazy`'s value is not read until the first request is made.
     *
     * This must be called
     * - after initializing a megazord for users using a custom megazord build.
     * - before any other calls into application-services rust code which make HTTP requests.
     */
    fun setClient(c: Lazy<Client>) {
        AppSvcHttpConfig.setClient(c)
    }
}
