/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import androidx.paging.DataSource
import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.mockk
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.concept.engine.permission.SitePermissionsStorage
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@OptIn(ExperimentalCoroutinesApi::class)
@RunWith(FenixRobolectricTestRunner::class)
class PermissionStorageTest {

    @Test
    fun `add permission`() = runTest {
        val sitePermissions: SitePermissions = mockk(relaxed = true)
        val sitePermissionsStorage: SitePermissionsStorage = mockk(relaxed = true)
        val storage = PermissionStorage(testContext, this.coroutineContext, sitePermissionsStorage)

        storage.add(sitePermissions, false)

        coVerify { sitePermissionsStorage.save(sitePermissions, private = false) }
    }

    @Test
    fun `add permission in privateBrowsing`() = runTest {
        val sitePermissions: SitePermissions = mockk(relaxed = true)
        val sitePermissionsStorage: SitePermissionsStorage = mockk(relaxed = true)
        val storage = PermissionStorage(testContext, this.coroutineContext, sitePermissionsStorage)

        storage.add(sitePermissions, true)

        coVerify { sitePermissionsStorage.save(sitePermissions, private = true) }
    }

    @Test
    fun `find sitePermissions by origin`() = runTest {
        val sitePermissions: SitePermissions = mockk(relaxed = true)
        val sitePermissionsStorage: SitePermissionsStorage = mockk(relaxed = true)
        val storage = PermissionStorage(testContext, this.coroutineContext, sitePermissionsStorage)

        coEvery { sitePermissionsStorage.findSitePermissionsBy(any(), any(), any()) } returns sitePermissions

        val result = storage.findSitePermissionsBy("origin", false)

        coVerify { sitePermissionsStorage.findSitePermissionsBy("origin", private = false) }

        assertEquals(sitePermissions, result)
    }

    @Test
    fun `update SitePermissions`() = runTest {
        val sitePermissions: SitePermissions = mockk(relaxed = true)
        val sitePermissionsStorage: SitePermissionsStorage = mockk(relaxed = true)
        val storage = PermissionStorage(testContext, this.coroutineContext, sitePermissionsStorage)

        storage.updateSitePermissions(sitePermissions, private = false)

        coVerify { sitePermissionsStorage.update(sitePermissions, private = false) }
    }

    @Test
    fun `get sitePermissions paged`() = runTest {
        val dataSource: DataSource.Factory<Int, SitePermissions> = mockk(relaxed = true)
        val sitePermissionsStorage: SitePermissionsStorage = mockk(relaxed = true)
        val storage = PermissionStorage(testContext, this.coroutineContext, sitePermissionsStorage)

        coEvery { sitePermissionsStorage.getSitePermissionsPaged() } returns dataSource

        val result = storage.getSitePermissionsPaged()

        coVerify { sitePermissionsStorage.getSitePermissionsPaged() }

        assertEquals(dataSource, result)
    }

    @Test
    fun `delete sitePermissions`() = runTest {
        val sitePermissions: SitePermissions = mockk(relaxed = true)
        val sitePermissionsStorage: SitePermissionsStorage = mockk(relaxed = true)
        val storage = PermissionStorage(testContext, this.coroutineContext, sitePermissionsStorage)

        storage.deleteSitePermissions(sitePermissions)

        coVerify { sitePermissionsStorage.remove(sitePermissions, private = false) }
    }

    @Test
    fun `delete all sitePermissions`() = runTest {
        val sitePermissionsStorage: SitePermissionsStorage = mockk(relaxed = true)
        val storage = PermissionStorage(testContext, this.coroutineContext, sitePermissionsStorage)

        storage.deleteAllSitePermissions()

        coVerify { sitePermissionsStorage.removeAll() }
    }
}
