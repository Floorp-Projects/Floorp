/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.logins

import mozilla.components.concept.storage.Login

/**
 * Conversion from a generic [Login] into its richer comrade within the 'logins' lib.
 */
fun Login.toDatabaseLogin(): mozilla.appservices.logins.Login {
    return mozilla.appservices.logins.Login(
        id = this.guid ?: "",
        username = this.username,
        password = this.password,
        hostname = this.origin,
        formSubmitUrl = this.formActionOrigin,
        httpRealm = this.httpRealm,
        timesUsed = this.timesUsed,
        timeCreated = this.timeCreated,
        timeLastUsed = this.timeLastUsed,
        timePasswordChanged = this.timePasswordChanged,
        usernameField = this.usernameField ?: "",
        passwordField = this.passwordField ?: ""
    )
}

/**
 * Conversion from a "native" login [Login] into its generic comrade.
 */
fun mozilla.appservices.logins.Login.toLogin(): Login {
    return Login(
        guid = this.id,
        origin = this.hostname,
        formActionOrigin = this.formSubmitUrl,
        httpRealm = this.httpRealm,
        username = this.username,
        password = this.password,
        timesUsed = this.timesUsed,
        timeCreated = this.timeCreated,
        timeLastUsed = this.timeLastUsed,
        timePasswordChanged = this.timePasswordChanged,
        usernameField = this.usernameField,
        passwordField = this.passwordField
    )
}
