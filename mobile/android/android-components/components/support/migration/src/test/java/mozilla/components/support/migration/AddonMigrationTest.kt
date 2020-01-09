/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.lang.IllegalArgumentException

@RunWith(AndroidJUnit4::class)
class AddonMigrationTest {

    @Test
    fun `No addons installed`() = runBlocking {
        val engine: Engine = mock()
        val callbackCaptor = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(callbackCaptor.capture(), any())).thenAnswer {
            callbackCaptor.value.invoke(emptyList())
        }

        val result = AddonMigration.migrate(engine)

        assertTrue(result is Result.Success)
        val migrationResult = (result as Result.Success).value
        assertTrue(migrationResult is AddonMigrationResult.Success.NoAddons)
    }

    @Test
    fun `All addons migrated`() = runBlocking {
        val addon1: WebExtension = mock()
        val addon2: WebExtension = mock()

        val engine: Engine = mock()
        val listSuccessCallback = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(listSuccessCallback.capture(), any())).thenAnswer {
            listSuccessCallback.value.invoke(listOf(addon1, addon2))
        }

        val addonCaptor = argumentCaptor<WebExtension>()
        val disableSuccessCallback = argumentCaptor<((WebExtension) -> Unit)>()
        whenever(engine.disableWebExtension(addonCaptor.capture(), any(), disableSuccessCallback.capture(), any()))
            .thenAnswer { disableSuccessCallback.value.invoke(addonCaptor.value) }

        val result = AddonMigration.migrate(engine)

        assertTrue(result is Result.Success)
        val migrationResult = (result as Result.Success).value
        assertTrue(migrationResult is AddonMigrationResult.Success.AddonsMigrated)

        val migratedAddons = (migrationResult as AddonMigrationResult.Success.AddonsMigrated).migratedAddons
        assertEquals(listOf(addon1, addon2), migratedAddons)
    }

    @Test
    fun `Failure when querying installed addons`() = runBlocking {
        val engine: Engine = mock()
        val errorCallback = argumentCaptor<((Throwable) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(any(), errorCallback.capture())).thenAnswer {
            errorCallback.value.invoke(IllegalArgumentException())
        }

        val result = AddonMigration.migrate(engine)

        assertTrue(result is Result.Failure)
        val migrationResult = (result as Result.Failure).throwables.first()
        assertTrue(migrationResult is AddonMigrationException)

        val failure = (migrationResult as AddonMigrationException).failure
        assertTrue(failure is AddonMigrationResult.Failure.FailedToQueryInstalledAddons)

        val cause = (failure as AddonMigrationResult.Failure.FailedToQueryInstalledAddons).throwable
        assertTrue(cause is IllegalArgumentException)
    }

    @Test
    fun `Failure when migrating some addons`() = runBlocking {
        val addon1: WebExtension = mock()
        val addon2: WebExtension = mock()
        val addon3: WebExtension = mock()

        val engine: Engine = mock()
        val listSuccessCallback = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(listSuccessCallback.capture(), any())).thenAnswer {
            listSuccessCallback.value.invoke(listOf(addon1, addon2, addon3))
        }

        val addonCaptor = argumentCaptor<WebExtension>()
        val disableSuccessCallback = argumentCaptor<((WebExtension) -> Unit)>()
        val disableErrorCallback = argumentCaptor<((Throwable) -> Unit)>()
        whenever(engine.disableWebExtension(
            addonCaptor.capture(),
            any(),
            disableSuccessCallback.capture(),
            disableErrorCallback.capture())
        )
        .thenAnswer {
            if (addonCaptor.value == addon2) {
                disableErrorCallback.value.invoke(IllegalArgumentException())
            } else {
                disableSuccessCallback.value.invoke(addonCaptor.value)
            }
        }

        val result = AddonMigration.migrate(engine)

        assertTrue(result is Result.Failure)
        val migrationResult = (result as Result.Failure).throwables.first()
        assertTrue(migrationResult is AddonMigrationException)

        val failure = (migrationResult as AddonMigrationException).failure
        assertTrue(failure is AddonMigrationResult.Failure.FailedToMigrateAddons)

        val migratedAddons = (failure as AddonMigrationResult.Failure.FailedToMigrateAddons).migratedAddons
        val failedAddons = failure.failedAddons
        assertEquals(listOf(addon1, addon3), migratedAddons)
        assertEquals(setOf(addon2), failedAddons.keys)
        assertTrue(failedAddons[addon2] is IllegalArgumentException)
    }
}
