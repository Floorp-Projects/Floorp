/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.storage

import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginsStorage
import org.json.JSONObject
import java.util.UUID

/**
 * A dummy [LoginsStorage] that returns a fixed set of fake logins. Currently sample browser does
 * not use an actual login storage backend.
 */
class DummyLoginsStorage : LoginsStorage {
    // A list of fake logins for testing purposes.
    private val logins = mutableListOf(
        Login(
            guid = "9282b6fd-97ba-4636-beca-975d9f4fd150",
            origin = "twitter.com",
            username = "NotAnAccount",
            password = "NotReallyAPassword"
        ),
        Login(
            guid = "8034a37f-5f9e-4136-95b2-e0293116b322",
            origin = "twitter.com",
            username = "",
            password = "NotReallyAPassword"
        )
    )

    override suspend fun touch(guid: String) = Unit

    override suspend fun delete(id: String): Boolean = logins.removeAll { login -> login.guid == id }

    override suspend fun get(guid: String): Login? = logins.first { login -> login.guid == guid }

    override suspend fun list(): List<Login> = logins

    override suspend fun wipe() = logins.clear()

    override suspend fun wipeLocal() = logins.clear()

    override suspend fun add(login: Login): String {
        val uuid = UUID.randomUUID().toString()
        logins.add(login.copy(guid = uuid))
        return uuid
    }

    override suspend fun update(login: Login) {
        logins.map { current ->
            if (current.guid == login.guid) {
                login
            } else {
                current
            }
        }
    }

    override suspend fun importLoginsAsync(logins: List<Login>) = JSONObject()

    override suspend fun ensureValid(login: Login) = Unit

    override suspend fun getByBaseDomain(origin: String): List<Login> {
        return logins.filter { login -> login.origin == origin }
    }

    override suspend fun getPotentialDupesIgnoringUsername(login: Login): List<Login> {
        return emptyList()
    }

    override fun close() = Unit
}
