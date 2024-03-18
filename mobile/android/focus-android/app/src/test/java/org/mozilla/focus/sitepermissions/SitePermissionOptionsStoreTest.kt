/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.sitepermissions

import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.verify
import org.mozilla.focus.settings.permissions.AutoplayOption
import org.mozilla.focus.settings.permissions.SitePermissionOption
import org.mozilla.focus.settings.permissions.permissionoptions.SitePermission
import org.mozilla.focus.settings.permissions.permissionoptions.SitePermissionOptionsScreenAction
import org.mozilla.focus.settings.permissions.permissionoptions.SitePermissionOptionsScreenState
import org.mozilla.focus.settings.permissions.permissionoptions.SitePermissionOptionsScreenStore
import org.mozilla.focus.settings.permissions.permissionoptions.SitePermissionOptionsStorage
import org.mozilla.focus.settings.permissions.permissionoptions.SitePermissionOptionsStorageMiddleware

class SitePermissionOptionsStoreTest {
    private lateinit var sitePermissionOptionsStorageMiddleware: SitePermissionOptionsStorageMiddleware
    private lateinit var store: SitePermissionOptionsScreenStore
    private lateinit var sitePermissionState: SitePermissionOptionsScreenState
    private val storage: SitePermissionOptionsStorage = mock()

    @Before
    fun setup() {
        sitePermissionOptionsStorageMiddleware = SitePermissionOptionsStorageMiddleware(SitePermission.CAMERA, storage)
        sitePermissionState = SitePermissionOptionsScreenState()

        doReturn(listOf(SitePermissionOption.AskToAllow(), SitePermissionOption.Blocked())).`when`(storage).getSitePermissionOptions(SitePermission.CAMERA)
        doReturn(SitePermissionOption.AskToAllow()).`when`(storage).permissionSelectedOption(SitePermission.CAMERA)
        doReturn("Camera").`when`(storage).getSitePermissionLabel(SitePermission.CAMERA)
        doReturn(false).`when`(storage).isAndroidPermissionGranted(SitePermission.CAMERA)

        store = SitePermissionOptionsScreenStore(sitePermissionState, listOf(sitePermissionOptionsStorageMiddleware))
    }

    @Test
    fun `GIVEN site permission screen store WHEN android permission action is dispatched THEN site permission options screen state is updated`() {
        store.dispatch(SitePermissionOptionsScreenAction.AndroidPermission(true)).joinBlocking()

        verify(storage).getSitePermissionOptions(SitePermission.CAMERA)
        verify(storage).permissionSelectedOption(SitePermission.CAMERA)
        verify(storage).getSitePermissionLabel(SitePermission.CAMERA)
        verify(storage).isAndroidPermissionGranted(SitePermission.CAMERA)
        assertTrue(store.state.isAndroidPermissionGranted)
    }

    @Test
    fun `GIVEN site permission screen store WHEN select permission action is dispatched THEN site permission options screen state is updated`() {
        store.dispatch(SitePermissionOptionsScreenAction.Select(SitePermissionOption.Blocked())).joinBlocking()

        verify(storage).saveCurrentSitePermissionOptionInSharePref(SitePermissionOption.Blocked(), SitePermission.CAMERA)
        assertEquals(SitePermissionOption.Blocked(), store.state.selectedSitePermissionOption)
    }

    @Test
    fun `GIVEN site permission screen store WHEN update site permission options is dispatched THEN site permission options screen state is updated`() {
        val sitePermissionLabel = "Autoplay"
        store.dispatch(
            SitePermissionOptionsScreenAction.UpdateSitePermissionOptions(
                listOf(
                    AutoplayOption.BlockAudioOnly(),
                    AutoplayOption.AllowAudioVideo(),
                    AutoplayOption.BlockAudioVideo(),
                ),
                AutoplayOption.AllowAudioVideo(),
                sitePermissionLabel,
                true,
            ),
        ).joinBlocking()

        assertEquals(AutoplayOption.AllowAudioVideo(), store.state.selectedSitePermissionOption)
        assertTrue(store.state.isAndroidPermissionGranted)
        assertEquals(
            listOf(
                AutoplayOption.BlockAudioOnly(),
                AutoplayOption.AllowAudioVideo(),
                AutoplayOption.BlockAudioVideo(),
            ),
            store.state.sitePermissionOptionList,
        )
        assertEquals(sitePermissionLabel, store.state.sitePermissionLabel)
    }
}
