/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.update.db

import mozilla.components.feature.addons.update.AddonUpdater
import mozilla.components.feature.addons.update.AddonUpdater.Status.Error
import mozilla.components.feature.addons.update.AddonUpdater.Status.NoUpdateAvailable
import mozilla.components.feature.addons.update.AddonUpdater.Status.NotInstalled
import mozilla.components.feature.addons.update.AddonUpdater.Status.SuccessfullyUpdated
import mozilla.components.feature.addons.update.db.UpdateAttemptEntity.Companion.ERROR_DB
import mozilla.components.feature.addons.update.db.UpdateAttemptEntity.Companion.NOT_INSTALLED_DB
import mozilla.components.feature.addons.update.db.UpdateAttemptEntity.Companion.NO_UPDATE_AVAILABLE_DB
import mozilla.components.feature.addons.update.db.UpdateAttemptEntity.Companion.SUCCESSFULLY_UPDATED_DB
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import java.util.Date

class UpdateAttemptEntityTest {

    @Test
    fun `convert from db entity to domain class`() {
        val dbEntity = UpdateAttemptEntity(
            addonId = "mozilla-dev",
            date = Date().time,
            status = SUCCESSFULLY_UPDATED_DB,
        )

        val domainClass = dbEntity.toUpdateAttempt()

        with(dbEntity) {
            assertEquals(addonId, domainClass.addonId)
            assertEquals(date, domainClass.date.time)
            assertEquals(SuccessfullyUpdated, domainClass.status)
        }
    }

    @Test
    fun `convert from domain class to db entity`() {
        val domainClass = AddonUpdater.UpdateAttempt(
            addonId = "mozilla-dev",
            date = Date(),
            status = Error("error", Exception()),
        )

        val dbEntity = domainClass.toEntity()

        with(dbEntity) {
            assertEquals(addonId, domainClass.addonId)
            assertEquals(date, domainClass.date.time)
            assertEquals(Error("error", Exception()).toString(), domainClass.status.toString())
            assertEquals(errorMessage, "error")
        }
    }

    @Test
    fun `convert from db status to domain status`() {
        var domainStatus: AddonUpdater.Status? = createDBUpdateAttempt(NOT_INSTALLED_DB).toStatus()

        assertEquals(NotInstalled, domainStatus)

        domainStatus = createDBUpdateAttempt(SUCCESSFULLY_UPDATED_DB).toStatus()

        assertEquals(SuccessfullyUpdated, domainStatus)

        domainStatus = createDBUpdateAttempt(NO_UPDATE_AVAILABLE_DB).toStatus()

        assertEquals(NoUpdateAvailable, domainStatus)

        domainStatus = createDBUpdateAttempt(ERROR_DB).toStatus()

        assertEquals(Error("error_message", Exception("error_trace")).toString(), domainStatus.toString())

        domainStatus = createDBUpdateAttempt(Int.MAX_VALUE).toStatus()

        assertNull(domainStatus)
    }

    @Test
    fun `convert from domain status to db status`() {
        val request = AddonUpdater.UpdateAttempt("id", Date(), status = NotInstalled)
        var dbStatus: Int = request.toEntity().status

        assertEquals(NOT_INSTALLED_DB, dbStatus)

        dbStatus = request.copy(status = SuccessfullyUpdated)
            .toEntity()
            .status

        assertEquals(SUCCESSFULLY_UPDATED_DB, dbStatus)

        dbStatus = request.copy(status = NoUpdateAvailable)
            .toEntity()
            .status

        assertEquals(NO_UPDATE_AVAILABLE_DB, dbStatus)

        val exception = Exception("")
        val dbEntity = AddonUpdater.UpdateAttempt(
            addonId = "id",
            date = Date(),
            status = Error("error message", exception),
        )
            .toEntity()

        assertEquals(ERROR_DB, dbEntity.status)
        assertEquals(exception.stackTrace.first().toString(), dbEntity.errorTrace)
    }

    private fun createDBUpdateAttempt(status: Int): UpdateAttemptEntity {
        return UpdateAttemptEntity("id", 1L, status, "error_message", "error_trace")
    }
}
