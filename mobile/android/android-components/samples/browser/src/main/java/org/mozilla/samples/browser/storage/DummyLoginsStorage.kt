/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.storage

import mozilla.components.concept.storage.EncryptedLogin
import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginEntry
import mozilla.components.concept.storage.LoginsStorage
import org.json.JSONObject
import java.util.UUID

fun makeLogin(guid: String, entry: LoginEntry) = Login(
    guid = guid,
    origin = entry.origin,
    httpRealm = entry.httpRealm,
    formActionOrigin = entry.formActionOrigin,
    username = entry.username,
    password = entry.password,
)

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

    override suspend fun delete(guid: String): Boolean = logins.removeAll { login -> login.guid == guid }

    override suspend fun get(guid: String): Login? = logins.first { login -> login.guid == guid }

    override suspend fun list(): List<Login> = logins

    override suspend fun wipe() = logins.clear()

    override suspend fun wipeLocal() = logins.clear()

    override suspend fun add(entry: LoginEntry): EncryptedLogin {
        val uuid = UUID.randomUUID().toString()
        val login = makeLogin(uuid, entry)
        logins.add(login)
        return encryptLogin(login)
    }

    override suspend fun update(guid: String, entry: LoginEntry): EncryptedLogin {
        var login: Login? = null
        logins.map { current ->
            if (current.guid == guid) {
                login = makeLogin(guid, entry)
            } else {
                current
            }
        }
        return encryptLogin(login!!)
    }

    // Simplified version of the findLoginToUpdate logic from application-services
    override fun findLoginToUpdate(entry: LoginEntry): Login? {
        return logins.firstOrNull { it.username == entry.username }
    }

    // Simplified version of the addOrUpdate logic from application-services
    override suspend fun addOrUpdate(entry: LoginEntry): EncryptedLogin {
        val existing = logins.firstOrNull {
            it.origin == entry.origin && it.username == entry.username
        }
        if (existing == null) {
            return add(entry)
        } else {
            return update(existing.guid, entry)
        }
    }

    override suspend fun importLoginsAsync(logins: List<Login>) = JSONObject()

    override suspend fun getByBaseDomain(origin: String): List<Login> {
        return logins.filter { login -> login.origin == origin }
    }

    // "Encryption" for us is just serializing to JSON
    private fun encryptLogin(login: Login): EncryptedLogin {
        var obj = JSONObject()
        obj.put("username", login.username)
        obj.put("password", login.password)
        return EncryptedLogin(
            guid = login.guid,
            origin = login.origin,
            httpRealm = login.httpRealm,
            formActionOrigin = login.formActionOrigin,
            secFields = obj.toString(),
        )
    }

    override fun decryptLogin(login: EncryptedLogin): Login {
        val obj = JSONObject(login.secFields)
        return Login(
            guid = login.guid,
            origin = login.origin,
            httpRealm = login.httpRealm,
            formActionOrigin = login.formActionOrigin,
            username = obj.getString("username"),
            password = obj.getString("password"),
        )
    }

    override fun close() = Unit
}
