/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.logins

import mozilla.components.concept.storage.EncryptedLogin
import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginEntry

// Convert between application-services data classes and the ones in concept.storage.

/**
 * Convert A-S EncryptedLogin into A-C [EncryptedLogin].
 */
fun mozilla.appservices.logins.EncryptedLogin.toEncryptedLogin() = EncryptedLogin(
    guid = record.id,
    origin = fields.origin,
    formActionOrigin = fields.formActionOrigin,
    httpRealm = fields.httpRealm,
    usernameField = fields.usernameField,
    passwordField = fields.passwordField,
    timesUsed = record.timesUsed,
    timeCreated = record.timeCreated,
    timeLastUsed = record.timeLastUsed,
    timePasswordChanged = record.timePasswordChanged,
    secFields = secFields,
)

/**
 * Convert A-S Login into A-C [Login].
 */
fun mozilla.appservices.logins.Login.toLogin() = Login(
    guid = record.id,
    origin = fields.origin,
    username = secFields.username,
    password = secFields.password,
    formActionOrigin = fields.formActionOrigin,
    httpRealm = fields.httpRealm,
    usernameField = fields.usernameField,
    passwordField = fields.passwordField,
    timesUsed = record.timesUsed,
    timeCreated = record.timeCreated,
    timeLastUsed = record.timeLastUsed,
    timePasswordChanged = record.timePasswordChanged,
)

/**
 * Convert A-C [LoginEntry] into A-S LoginEntry.
 */
fun LoginEntry.toLoginEntry() = mozilla.appservices.logins.LoginEntry(
    fields = mozilla.appservices.logins.LoginFields(
        origin = origin,
        formActionOrigin = formActionOrigin,
        httpRealm = httpRealm,
        usernameField = usernameField,
        passwordField = passwordField,
    ),
    secFields = mozilla.appservices.logins.SecureLoginFields(
        username = username,
        password = password,
    ),
)

/**
 * Convert A-C [Login] into A-S Login.
 */
fun Login.toLogin() = mozilla.appservices.logins.Login(
    record = mozilla.appservices.logins.RecordFields(
        id = guid,
        timesUsed = timesUsed,
        timeCreated = timeCreated,
        timeLastUsed = timeLastUsed,
        timePasswordChanged = timePasswordChanged,
    ),
    fields = mozilla.appservices.logins.LoginFields(
        origin = origin,
        formActionOrigin = formActionOrigin,
        httpRealm = httpRealm,
        usernameField = usernameField,
        passwordField = passwordField,
    ),
    secFields = mozilla.appservices.logins.SecureLoginFields(
        username = username,
        password = password,
    ),
)
