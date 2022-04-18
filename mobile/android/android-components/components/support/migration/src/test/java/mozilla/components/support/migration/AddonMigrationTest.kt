/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.webextension.DisabledFlags
import mozilla.components.concept.engine.webextension.Metadata
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.amo.AddonCollectionProvider
import mozilla.components.feature.addons.update.AddonUpdater
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class AddonMigrationTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `No addons installed`() = runBlocking {
        val engine: Engine = mock()
        val callbackCaptor = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(callbackCaptor.capture(), any())).thenAnswer {
            callbackCaptor.value.invoke(emptyList())
        }

        val result = AddonMigration.migrate(engine, mock(), mock())

        assertTrue(result is Result.Success)
        val migrationResult = (result as Result.Success).value
        assertTrue(migrationResult is AddonMigrationResult.Success.NoAddons)
    }

    @Test
    fun `All addons migrated successfully`() = runBlocking {
        val addon1: WebExtension = mock()
        whenever(addon1.id).thenReturn("addon1")
        whenever(addon1.isBuiltIn()).thenReturn(false)

        val addon2: WebExtension = mock()
        whenever(addon2.id).thenReturn("addon2")
        whenever(addon2.isBuiltIn()).thenReturn(false)

        // Built-in add-ons (browser-icons, readerview) do not need to and shouldn't be migrated.
        val addon3: WebExtension = mock()
        whenever(addon3.isBuiltIn()).thenReturn(true)

        val engine: Engine = mock()
        val addonCollectionProvider: AddonCollectionProvider = mock()
        val supportedAddons = listOf(Addon(addon1.id), Addon(addon2.id))
        whenever(
            addonCollectionProvider.getAvailableAddons(anyBoolean(), eq(AMO_READ_TIMEOUT_IN_SECONDS), language = anyString())
        ).thenReturn(supportedAddons)

        val listSuccessCallback = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(listSuccessCallback.capture(), any())).thenAnswer {
            listSuccessCallback.value.invoke(listOf(addon1, addon2, addon3))
        }

        val addonCaptor = argumentCaptor<WebExtension>()
        val disableSuccessCallback = argumentCaptor<((WebExtension) -> Unit)>()
        whenever(engine.disableWebExtension(addonCaptor.capture(), any(), disableSuccessCallback.capture(), any()))
            .thenAnswer { disableSuccessCallback.value.invoke(addonCaptor.value) }

        val result = AddonMigration.migrate(engine, addonCollectionProvider, mock())

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

        val result = AddonMigration.migrate(engine, mock(), mock())

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
        whenever(
            engine.disableWebExtension(
                addonCaptor.capture(),
                any(),
                disableSuccessCallback.capture(),
                disableErrorCallback.capture()
            )
        )
            .thenAnswer {
                if (addonCaptor.value == addon2) {
                    disableErrorCallback.value.invoke(IllegalArgumentException())
                } else {
                    disableSuccessCallback.value.invoke(addonCaptor.value)
                }
            }

        val result = AddonMigration.migrate(engine, mock(), mock())

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

    @Test
    fun `Unsupported addons are disabled during migration`() = runBlocking {
        val addon1: WebExtension = mock()
        whenever(addon1.isBuiltIn()).thenReturn(false)

        val addon2: WebExtension = mock()
        whenever(addon2.isBuiltIn()).thenReturn(false)

        val engine: Engine = mock()
        val addonCollectionProvider: AddonCollectionProvider = mock()
        // No supported add-on
        whenever(
            addonCollectionProvider.getAvailableAddons(anyBoolean(), eq(AMO_READ_TIMEOUT_IN_SECONDS), language = anyString())
        ).thenReturn(emptyList())

        val listSuccessCallback = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(listSuccessCallback.capture(), any())).thenAnswer {
            listSuccessCallback.value.invoke(listOf(addon1, addon2))
        }

        val addonCaptor = argumentCaptor<WebExtension>()
        val disableSuccessCallback = argumentCaptor<((WebExtension) -> Unit)>()
        whenever(engine.disableWebExtension(addonCaptor.capture(), any(), disableSuccessCallback.capture(), any()))
            .thenAnswer { disableSuccessCallback.value.invoke(addonCaptor.value) }

        val result = AddonMigration.migrate(engine, addonCollectionProvider, mock())

        assertTrue(result is Result.Success)
        val migrationResult = (result as Result.Success).value
        assertTrue(migrationResult is AddonMigrationResult.Success.AddonsMigrated)

        val migratedAddons = (migrationResult as AddonMigrationResult.Success.AddonsMigrated).migratedAddons
        assertEquals(listOf(addon1, addon2), migratedAddons)
    }

    @Test
    fun `Supported addons remain enabled during migration`() = runBlocking {
        val addon1: WebExtension = mock()
        whenever(addon1.id).thenReturn("supportedAddon")
        whenever(addon1.isEnabled()).thenReturn(true)
        whenever(addon1.isBuiltIn()).thenReturn(false)

        val addon2: WebExtension = mock()
        whenever(addon2.id).thenReturn("unsupportedAddon")
        whenever(addon2.isEnabled()).thenReturn(true)
        whenever(addon2.isBuiltIn()).thenReturn(false)

        val engine: Engine = mock()
        val addonCollectionProvider: AddonCollectionProvider = mock()
        val supportedAddons = listOf(Addon(addon1.id))
        whenever(addonCollectionProvider.getAvailableAddons()).thenReturn(supportedAddons)

        val listSuccessCallback = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(listSuccessCallback.capture(), any())).thenAnswer {
            listSuccessCallback.value.invoke(listOf(addon1, addon2))
        }

        val disableAddonCaptor = argumentCaptor<WebExtension>()
        val disableSuccessCallback = argumentCaptor<((WebExtension) -> Unit)>()
        whenever(engine.disableWebExtension(disableAddonCaptor.capture(), any(), disableSuccessCallback.capture(), any()))
            .thenAnswer {
                val disabledAddon: WebExtension = mock()
                val id = disableAddonCaptor.value.id
                whenever(disabledAddon.id).thenReturn(id)
                whenever(disabledAddon.isEnabled()).thenReturn(false)
                disableSuccessCallback.value.invoke(disabledAddon)
            }

        val addonUpdater: AddonUpdater = mock()
        val result = AddonMigration.migrate(engine, addonCollectionProvider, addonUpdater)

        assertTrue(result is Result.Success)
        val migrationResult = (result as Result.Success).value
        assertTrue(migrationResult is AddonMigrationResult.Success.AddonsMigrated)

        val migratedAddons = (migrationResult as AddonMigrationResult.Success.AddonsMigrated).migratedAddons
        assertEquals(2, migratedAddons.size)
        // Supported add-on should still be enabled and updater should be scheduled
        assertEquals(migratedAddons[0].id, addon1.id)
        assertTrue(migratedAddons[0].isEnabled())
        verify(addonUpdater).registerForFutureUpdates(addon1.id)

        // Unsupported add-on should be disabled now
        assertEquals(migratedAddons[1].id, addon2.id)
        assertFalse(migratedAddons[1].isEnabled())
    }

    @Test
    fun `Previously unsupported addons are enabled during migration if supported`() = runBlocking {
        val metadata: Metadata = mock()
        whenever(metadata.disabledFlags).thenReturn(DisabledFlags.select(DisabledFlags.APP_SUPPORT))

        val addon: WebExtension = mock()
        whenever(addon.id).thenReturn("supportedAddon")
        whenever(addon.isEnabled()).thenReturn(false)
        whenever(addon.isBuiltIn()).thenReturn(false)
        whenever(addon.getMetadata()).thenReturn(metadata)

        val engine: Engine = mock()
        val addonCollectionProvider: AddonCollectionProvider = mock()
        val supportedAddons = listOf(Addon(addon.id))
        whenever(addonCollectionProvider.getAvailableAddons()).thenReturn(supportedAddons)

        val listSuccessCallback = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(listSuccessCallback.capture(), any())).thenAnswer {
            listSuccessCallback.value.invoke(listOf(addon))
        }

        val enableAddonCaptor = argumentCaptor<WebExtension>()
        val enableSuccessCallback = argumentCaptor<((WebExtension) -> Unit)>()
        whenever(engine.enableWebExtension(enableAddonCaptor.capture(), any(), enableSuccessCallback.capture(), any()))
            .thenAnswer {
                val enabledAddon: WebExtension = mock()
                val id = enableAddonCaptor.value.id
                whenever(enabledAddon.id).thenReturn(id)
                whenever(enabledAddon.isEnabled()).thenReturn(true)
                enableSuccessCallback.value.invoke(enabledAddon)
            }

        val addonUpdater: AddonUpdater = mock()
        val result = AddonMigration.migrate(engine, addonCollectionProvider, addonUpdater)

        assertTrue(result is Result.Success)
        val migrationResult = (result as Result.Success).value
        assertTrue(migrationResult is AddonMigrationResult.Success.AddonsMigrated)

        val migratedAddons = (migrationResult as AddonMigrationResult.Success.AddonsMigrated).migratedAddons
        assertEquals(1, migratedAddons.size)
        assertEquals(migratedAddons[0].id, addon.id)
        assertTrue(migratedAddons[0].isEnabled())
        verify(addonUpdater).registerForFutureUpdates(addon.id)
    }

    @Test
    fun `Fall back to hardcoded list of supported addons`() = runBlocking {
        val addon1: WebExtension = mock()
        whenever(addon1.id).thenReturn("supportedAddon")
        whenever(addon1.isEnabled()).thenReturn(true)
        whenever(addon1.isBuiltIn()).thenReturn(false)

        val addon2: WebExtension = mock()
        whenever(addon2.id).thenReturn("unsupportedAddon")
        whenever(addon2.isEnabled()).thenReturn(true)
        whenever(addon2.isBuiltIn()).thenReturn(false)

        supportedAddonsFallback = listOf(addon1.id)
        val engine: Engine = mock()
        val addonCollectionProvider: AddonCollectionProvider = mock()
        // Throw an exception to trigger fallback to hard-coded list
        whenever(addonCollectionProvider.getAvailableAddons()).thenThrow(IllegalArgumentException())

        val listSuccessCallback = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(listSuccessCallback.capture(), any())).thenAnswer {
            listSuccessCallback.value.invoke(listOf(addon1, addon2))
        }

        val disableAddonCaptor = argumentCaptor<WebExtension>()
        val disableSuccessCallback = argumentCaptor<((WebExtension) -> Unit)>()
        whenever(engine.disableWebExtension(disableAddonCaptor.capture(), any(), disableSuccessCallback.capture(), any()))
            .thenAnswer {
                val disabledAddon: WebExtension = mock()
                val id = disableAddonCaptor.value.id
                whenever(disabledAddon.id).thenReturn(id)
                whenever(disabledAddon.isEnabled()).thenReturn(false)
                disableSuccessCallback.value.invoke(disabledAddon)
            }

        val addonUpdater: AddonUpdater = mock()
        val result = AddonMigration.migrate(engine, addonCollectionProvider, addonUpdater)

        assertTrue(result is Result.Success)
        val migrationResult = (result as Result.Success).value
        assertTrue(migrationResult is AddonMigrationResult.Success.AddonsMigrated)

        val migratedAddons = (migrationResult as AddonMigrationResult.Success.AddonsMigrated).migratedAddons
        assertEquals(2, migratedAddons.size)
        // Supported add-on should still be enabled and updater should be scheduled
        assertEquals(migratedAddons[0].id, addon1.id)
        assertTrue(migratedAddons[0].isEnabled())
        verify(addonUpdater).registerForFutureUpdates(addon1.id)

        // Unsupported add-on should be disabled now
        assertEquals(migratedAddons[1].id, addon2.id)
        assertFalse(migratedAddons[1].isEnabled())
    }
}
