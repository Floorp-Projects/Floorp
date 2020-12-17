/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import androidx.paging.DataSource
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.feature.sitepermissions.SitePermissionsStorage
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@OptIn(ExperimentalCoroutinesApi::class)
@RunWith(FenixRobolectricTestRunner::class)
class PermissionStorageTest {

    @Test
    fun `add permission`() = runBlockingTest {
        val sitePermissions: SitePermissions = mockk(relaxed = true)
        val sitePermissionsStorage: SitePermissionsStorage = mockk(relaxed = true)
        val storage = PermissionStorage(testContext, this.coroutineContext, sitePermissionsStorage)

        storage.add(sitePermissions)

        verify { sitePermissionsStorage.save(sitePermissions) }
    }

    @Test
    fun `find sitePermissions by origin`() = runBlockingTest {
        val sitePermissions: SitePermissions = mockk(relaxed = true)
        val sitePermissionsStorage: SitePermissionsStorage = mockk(relaxed = true)
        val storage = PermissionStorage(testContext, this.coroutineContext, sitePermissionsStorage)

        every { sitePermissionsStorage.findSitePermissionsBy(any()) } returns sitePermissions

        val result = storage.findSitePermissionsBy("origin")

        verify { sitePermissionsStorage.findSitePermissionsBy("origin") }

        assertEquals(sitePermissions, result)
    }

    @Test
    fun `update SitePermissions`() = runBlockingTest {
        val sitePermissions: SitePermissions = mockk(relaxed = true)
        val sitePermissionsStorage: SitePermissionsStorage = mockk(relaxed = true)
        val storage = PermissionStorage(testContext, this.coroutineContext, sitePermissionsStorage)

        storage.updateSitePermissions(sitePermissions)

        verify { sitePermissionsStorage.update(sitePermissions) }
    }

    @Test
    fun `get sitePermissions paged`() = runBlockingTest {
        val dataSource: DataSource.Factory<Int, SitePermissions> = mockk(relaxed = true)
        val sitePermissionsStorage: SitePermissionsStorage = mockk(relaxed = true)
        val storage = PermissionStorage(testContext, this.coroutineContext, sitePermissionsStorage)

        every { sitePermissionsStorage.getSitePermissionsPaged() } returns dataSource

        val result = storage.getSitePermissionsPaged()

        verify { sitePermissionsStorage.getSitePermissionsPaged() }

        assertEquals(dataSource, result)
    }

    @Test
    fun `delete sitePermissions`() = runBlockingTest {
        val sitePermissions: SitePermissions = mockk(relaxed = true)
        val sitePermissionsStorage: SitePermissionsStorage = mockk(relaxed = true)
        val storage = PermissionStorage(testContext, this.coroutineContext, sitePermissionsStorage)

        storage.deleteSitePermissions(sitePermissions)

        verify { sitePermissionsStorage.remove(sitePermissions) }
    }

    @Test
    fun `delete all sitePermissions`() = runBlockingTest {
        val sitePermissionsStorage: SitePermissionsStorage = mockk(relaxed = true)
        val storage = PermissionStorage(testContext, this.coroutineContext, sitePermissionsStorage)

        storage.deleteAllSitePermissions()

        verify { sitePermissionsStorage.removeAll() }
    }
}
