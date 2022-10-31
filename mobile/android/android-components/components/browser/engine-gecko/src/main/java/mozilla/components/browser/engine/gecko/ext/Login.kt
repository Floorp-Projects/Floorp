/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.ext

import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginEntry
import org.mozilla.geckoview.Autocomplete

/**
 * Converts a GeckoView [Autocomplete.LoginEntry] to an Android Components [LoginEntry].
 */
fun Autocomplete.LoginEntry.toLoginEntry() = LoginEntry(
    origin = origin,
    formActionOrigin = formActionOrigin,
    httpRealm = httpRealm,
    username = username,
    password = password,
)

/**
 * Converts an Android Components [Login] to a GeckoView [Autocomplete.LoginEntry].
 */
fun Login.toLoginEntry() = Autocomplete.LoginEntry.Builder()
    .guid(guid)
    .origin(origin)
    .formActionOrigin(formActionOrigin)
    .httpRealm(httpRealm)
    .username(username)
    .password(password)
    .build()

/**
 * Converts an Android Components [LoginEntry] to a GeckoView [Autocomplete.LoginEntry].
 */
fun LoginEntry.toLoginEntry() = Autocomplete.LoginEntry.Builder()
    .origin(origin)
    .formActionOrigin(formActionOrigin)
    .httpRealm(httpRealm)
    .username(username)
    .password(password)
    .build()
