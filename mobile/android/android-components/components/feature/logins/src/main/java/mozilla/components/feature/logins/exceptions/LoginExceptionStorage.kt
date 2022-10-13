/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.logins.exceptions

import android.content.Context
import androidx.paging.DataSource
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map
import mozilla.components.feature.logins.exceptions.adapter.LoginExceptionAdapter
import mozilla.components.feature.logins.exceptions.db.LoginExceptionDatabase
import mozilla.components.feature.logins.exceptions.db.LoginExceptionEntity
import mozilla.components.feature.prompts.login.LoginExceptions

/**
 * A storage implementation for organizing login exceptions.
 */
class LoginExceptionStorage(
    context: Context,
) : LoginExceptions {
    internal var database: Lazy<LoginExceptionDatabase> =
        lazy { LoginExceptionDatabase.get(context) }

    /**
     * Adds a new [LoginException].
     *
     * @param origin The origin.
     */
    override fun addLoginException(origin: String) {
        LoginExceptionEntity(
            origin = origin,
        ).also { entity ->
            entity.id = database.value.loginExceptionDao().insertLoginException(entity)
        }
    }

    /**
     * Returns a [Flow] list of all the [LoginException] instances.
     */
    fun getLoginExceptions(): Flow<List<LoginException>> {
        return database.value.loginExceptionDao().getLoginExceptions().map { list ->
            list.map { entity -> LoginExceptionAdapter(entity) }
        }
    }

    /**
     * Returns all [LoginException]s as a [DataSource.Factory].
     */
    fun getLoginExceptionsPaged(): DataSource.Factory<Int, LoginException> = database.value
        .loginExceptionDao()
        .getLoginExceptionsPaged()
        .map { entity -> LoginExceptionAdapter(entity) }

    /**
     * Removes the given [LoginException].
     */
    fun removeLoginException(site: LoginException) {
        val exceptionEntity = (site as LoginExceptionAdapter).entity
        database.value.loginExceptionDao().deleteLoginException(exceptionEntity)
    }

    override fun isLoginExceptionByOrigin(origin: String): Boolean {
        return findExceptionByOrigin(origin) != null
    }

    /**
     * Finds a [LoginException] by origin.
     */
    fun findExceptionByOrigin(origin: String): LoginException? {
        val exception = database.value.loginExceptionDao().findExceptionByOrigin(origin)
        return exception?.let {
            LoginExceptionAdapter(
                it,
            )
        }
    }

    /**
     * Removes all [LoginException]s.
     */
    fun deleteAllLoginExceptions() {
        database.value.loginExceptionDao().deleteAllLoginExceptions()
    }
}
