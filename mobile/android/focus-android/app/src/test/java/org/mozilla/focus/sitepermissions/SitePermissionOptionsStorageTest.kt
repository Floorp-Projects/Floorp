/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.sitepermissions

import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mozilla.focus.R
import org.mozilla.focus.settings.permissions.AutoplayOption
import org.mozilla.focus.settings.permissions.SitePermissionOption
import org.mozilla.focus.settings.permissions.permissionoptions.SitePermission
import org.mozilla.focus.settings.permissions.permissionoptions.SitePermissionOptionsStorage
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class SitePermissionOptionsStorageTest {
    private lateinit var storage: SitePermissionOptionsStorage

    @Before
    fun setup() {
        storage = spy(SitePermissionOptionsStorage(testContext))
    }

    @Test
    fun `GIVEN get site permission option selected label is called WHEN camera permission isn't granted THEN blocked by android is return`() {
        doReturn(false).`when`(storage).isAndroidPermissionGranted(SitePermission.CAMERA)

        assertEquals(
            testContext.getString(R.string.phone_feature_blocked_by_android),
            storage.getSitePermissionOptionSelectedLabel(SitePermission.CAMERA),
        )
    }

    @Test
    fun `GIVEN get site permission option selected label is called WHEN camera permission is granted THEN default value ask to allow option is return`() {
        doReturn(true).`when`(storage).isAndroidPermissionGranted(SitePermission.CAMERA)

        assertEquals(
            testContext.getString(R.string.preference_option_phone_feature_ask_to_allow),
            storage.getSitePermissionOptionSelectedLabel(SitePermission.CAMERA),
        )
    }

    @Test
    fun `GIVEN get site permission option selected label is called WHEN autoplay permission is granted THEN default value block audio only return`() {
        doReturn(true).`when`(storage).isAndroidPermissionGranted(SitePermission.AUTOPLAY)

        assertEquals(
            testContext.getString(R.string.preference_block_autoplay_audio_only),
            storage.getSitePermissionOptionSelectedLabel(SitePermission.AUTOPLAY),
        )
    }

    @Test
    fun `GIVEN get site permission option selected label is called WHEN location permission is granted THEN default value ask to allow option is return`() {
        doReturn(true).`when`(storage).isAndroidPermissionGranted(SitePermission.LOCATION)

        assertEquals(
            testContext.getString(R.string.preference_option_phone_feature_ask_to_allow),
            storage.getSitePermissionOptionSelectedLabel(SitePermission.LOCATION),
        )
    }

    @Test
    fun `GIVEN get site permission options is called WHEN location permission is passed as argument THEN available options are return`() {
        assertEquals(
            listOf(SitePermissionOption.AskToAllow(), SitePermissionOption.Blocked()),
            storage.getSitePermissionOptions(SitePermission.LOCATION),
        )
    }

    @Test
    fun `GIVEN get site permission options is called WHEN camera permission is passed as argument THEN available options are return`() {
        assertEquals(
            listOf(SitePermissionOption.AskToAllow(), SitePermissionOption.Blocked()),
            storage.getSitePermissionOptions(SitePermission.CAMERA),
        )
    }

    @Test
    fun `GIVEN get site permission options is called WHEN microphone permission is passed as argument THEN available options are return`() {
        assertEquals(
            listOf(SitePermissionOption.AskToAllow(), SitePermissionOption.Blocked()),
            storage.getSitePermissionOptions(SitePermission.MICROPHONE),
        )
    }

    @Test
    fun `GIVEN get site permission options is called WHEN notification permission is passed as argument THEN available options are return`() {
        assertEquals(
            listOf(SitePermissionOption.AskToAllow(), SitePermissionOption.Blocked()),
            storage.getSitePermissionOptions(SitePermission.NOTIFICATION),
        )
    }

    @Test
    fun `GIVEN get site permission options is called WHEN media key system access permission is passed as argument THEN available options are return`() {
        assertEquals(
            listOf(SitePermissionOption.AskToAllow(), SitePermissionOption.Blocked(), SitePermissionOption.Allowed()),
            storage.getSitePermissionOptions(SitePermission.MEDIA_KEY_SYSTEM_ACCESS),
        )
    }

    @Test
    fun `GIVEN get site permission options is called WHEN autoplay permission is passed as argument THEN available options are return`() {
        assertEquals(
            listOf(AutoplayOption.AllowAudioVideo(), AutoplayOption.BlockAudioOnly(), AutoplayOption.BlockAudioVideo()),
            storage.getSitePermissionOptions(SitePermission.AUTOPLAY),
        )
    }

    @Test
    fun `GIVEN get site permission label is called WHEN camera permission is passed as argument THEN permission label is return`() {
        assertEquals(
            testContext.getString(R.string.preference_phone_feature_camera),
            storage.getSitePermissionLabel(SitePermission.CAMERA),
        )
    }

    @Test
    fun `GIVEN get site permission label is called WHEN location permission is passed as argument THEN permission label is return`() {
        assertEquals(
            testContext.getString(R.string.preference_phone_feature_location),
            storage.getSitePermissionLabel(SitePermission.LOCATION),
        )
    }

    @Test
    fun `GIVEN get site permission label is called WHEN microphone permission is passed as argument THEN permission label is return`() {
        assertEquals(
            testContext.getString(R.string.preference_phone_feature_microphone),
            storage.getSitePermissionLabel(SitePermission.MICROPHONE),
        )
    }

    @Test
    fun `GIVEN get site permission label is called WHEN notification permission is passed as argument THEN permission label is return`() {
        assertEquals(
            testContext.getString(R.string.preference_phone_feature_notification),
            storage.getSitePermissionLabel(SitePermission.NOTIFICATION),
        )
    }

    @Test
    fun `GIVEN get site permission label is called WHEN media key system access permission is passed as argument THEN permission label is return`() {
        assertEquals(
            testContext.getString(R.string.preference_phone_feature_media_key_system_access),
            storage.getSitePermissionLabel(SitePermission.MEDIA_KEY_SYSTEM_ACCESS),
        )
    }

    @Test
    fun `GIVEN get site permission label is called WHEN autoplay permission is passed as argument THEN permission label is return`() {
        assertEquals(
            testContext.getString(R.string.preference_autoplay),
            storage.getSitePermissionLabel(SitePermission.AUTOPLAY),
        )
    }

    @Test
    fun `GIVEN get site permission default option is called WHEN camera permission is passed as argument THEN default option is return`() {
        assertEquals(
            SitePermissionOption.AskToAllow(),
            storage.getSitePermissionDefaultOption(SitePermission.CAMERA),
        )
    }

    @Test
    fun `GIVEN get site permission default option is called WHEN location permission is passed as argument THEN default option is return`() {
        assertEquals(
            SitePermissionOption.AskToAllow(),
            storage.getSitePermissionDefaultOption(SitePermission.LOCATION),
        )
    }

    @Test
    fun `GIVEN get site permission default option is called WHEN microphone permission is passed as argument THEN default option is return`() {
        assertEquals(
            SitePermissionOption.AskToAllow(),
            storage.getSitePermissionDefaultOption(SitePermission.MICROPHONE),
        )
    }

    @Test
    fun `GIVEN get site permission default option is called WHEN notification permission is passed as argument THEN default option is return`() {
        assertEquals(
            SitePermissionOption.AskToAllow(),
            storage.getSitePermissionDefaultOption(SitePermission.NOTIFICATION),
        )
    }

    @Test
    fun `GIVEN get site permission default option is called WHEN media key system access permission is passed as argument THEN default option is return`() {
        assertEquals(
            SitePermissionOption.AskToAllow(),
            storage.getSitePermissionDefaultOption(SitePermission.MEDIA_KEY_SYSTEM_ACCESS),
        )
    }

    @Test
    fun `GIVEN get site permission default option is called WHEN autoplay permission is passed as argument THEN default option is return`() {
        assertEquals(
            AutoplayOption.BlockAudioOnly(),
            storage.getSitePermissionDefaultOption(SitePermission.AUTOPLAY),
        )
    }

    @Test
    fun `GIVEN get permission selected option is called WHEN pref_key_allowed is saved in SharedPreferences THEN site permission Allowed is return`() {
        doReturn(testContext.getString(R.string.pref_key_allowed))
            .`when`(storage).permissionSelectedOptionByKey(R.string.pref_key_phone_feature_camera)

        assertEquals(
            SitePermissionOption.Allowed(),
            storage.permissionSelectedOption(SitePermission.CAMERA),
        )
    }

    @Test
    fun `GIVEN get permission selected option is called WHEN pref_key_allow_autoplay_audio_video is saved in SharedPreferences THEN site permission Allow Audio and Video is return`() {
        doReturn(testContext.getString(R.string.pref_key_allow_autoplay_audio_video))
            .`when`(storage).permissionSelectedOptionByKey(R.string.pref_key_phone_feature_camera)

        assertEquals(
            AutoplayOption.AllowAudioVideo(),
            storage.permissionSelectedOption(SitePermission.CAMERA),
        )
    }

    @Test
    fun `GIVEN get permission selected option is called WHEN pref_key_block_autoplay_audio_video is saved in SharedPreferences THEN site permission Block Audio and Video is return`() {
        doReturn(testContext.getString(R.string.pref_key_block_autoplay_audio_video))
            .`when`(storage).permissionSelectedOptionByKey(R.string.pref_key_phone_feature_camera)

        assertEquals(
            AutoplayOption.BlockAudioVideo(),
            storage.permissionSelectedOption(SitePermission.CAMERA),
        )
    }

    @Test
    fun `GIVEN get permission selected option is called WHEN pref_key_blocked is saved in SharedPreferences THEN site permission Blocked is return`() {
        doReturn(testContext.getString(R.string.pref_key_blocked))
            .`when`(storage).permissionSelectedOptionByKey(R.string.pref_key_phone_feature_camera)

        assertEquals(
            SitePermissionOption.Blocked(),
            storage.permissionSelectedOption(SitePermission.CAMERA),
        )
    }

    @Test
    fun `GIVEN get permission selected option is called WHEN pref_key_ask_to_allow is saved in SharedPreferences THEN site permission Ask to allow is return`() {
        doReturn(testContext.getString(R.string.pref_key_ask_to_allow))
            .`when`(storage).permissionSelectedOptionByKey(R.string.pref_key_phone_feature_camera)

        assertEquals(
            SitePermissionOption.AskToAllow(),
            storage.permissionSelectedOption(SitePermission.CAMERA),
        )
    }

    @Test
    fun `GIVEN get permission selected option is called WHEN pref_key_block_autoplay_audio_only is saved in SharedPreferences THEN site permission Block Audio only is return`() {
        doReturn(testContext.getString(R.string.pref_key_block_autoplay_audio_only))
            .`when`(storage).permissionSelectedOptionByKey(R.string.pref_key_phone_feature_camera)

        assertEquals(
            AutoplayOption.BlockAudioOnly(),
            storage.permissionSelectedOption(SitePermission.CAMERA),
        )
    }

    @Test
    fun `GIVEN get site permission preference id is called WHEN site permission camera is passed as argument THEN pref_key_phone_feature_camera is return`() {
        assertEquals(
            R.string.pref_key_phone_feature_camera,
            storage.getSitePermissionPreferenceId(SitePermission.CAMERA),
        )
    }

    @Test
    fun `GIVEN get site permission preference id is called WHEN site permission location is passed as argument THEN pref_key_phone_feature_location is return`() {
        assertEquals(
            R.string.pref_key_phone_feature_location,
            storage.getSitePermissionPreferenceId(SitePermission.LOCATION),
        )
    }

    @Test
    fun `GIVEN get site permission preference id is called WHEN site permission microphone is passed as argument THEN pref_key_phone_feature_microphone is return`() {
        assertEquals(
            R.string.pref_key_phone_feature_microphone,
            storage.getSitePermissionPreferenceId(SitePermission.MICROPHONE),
        )
    }

    @Test
    fun `GIVEN get site permission preference id is called WHEN site permission notification is passed as argument THEN pref_key_phone_feature_notification is return`() {
        assertEquals(
            R.string.pref_key_phone_feature_notification,
            storage.getSitePermissionPreferenceId(SitePermission.NOTIFICATION),
        )
    }

    @Test
    fun `GIVEN get site permission preference id is called WHEN site permission autoplay is passed as argument THEN pref_key_autoplay is return`() {
        assertEquals(
            R.string.pref_key_autoplay,
            storage.getSitePermissionPreferenceId(SitePermission.AUTOPLAY),
        )
    }

    @Test
    fun `GIVEN get site permission preference id is called WHEN site permission autoplay audible is passed as argument THEN pref_key_allow_autoplay_audio_video is return`() {
        assertEquals(
            R.string.pref_key_allow_autoplay_audio_video,
            storage.getSitePermissionPreferenceId(SitePermission.AUTOPLAY_AUDIBLE),
        )
    }

    @Test
    fun `GIVEN get site permission preference id is called WHEN site permission autoplay inaudible is passed as argument THEN pref_key_block_autoplay_audio_video is return`() {
        assertEquals(
            R.string.pref_key_block_autoplay_audio_video,
            storage.getSitePermissionPreferenceId(SitePermission.AUTOPLAY_INAUDIBLE),
        )
    }

    @Test
    fun `GIVEN get site permission preference id is called WHEN site permission media key system access is passed as argument THEN pref_key_browser_feature_media_key_system_access is return`() {
        assertEquals(
            R.string.pref_key_browser_feature_media_key_system_access,
            storage.getSitePermissionPreferenceId(SitePermission.MEDIA_KEY_SYSTEM_ACCESS),
        )
    }
}
