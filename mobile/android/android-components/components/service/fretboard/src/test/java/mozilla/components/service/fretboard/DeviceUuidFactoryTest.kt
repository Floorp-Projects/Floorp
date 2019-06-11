/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard

import android.content.Context
import android.content.SharedPreferences
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.any
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.anyString
import org.mockito.ArgumentMatchers.eq
import org.mockito.Mockito.`when`
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class DeviceUuidFactoryTest {

    @Test
    fun uuidNoPreference() {
        val context = mock<Context>()
        val sharedPreferences = mock<SharedPreferences>()
        val editor = mock<SharedPreferences.Editor>()
        `when`(editor.putString(anyString(), any())).thenReturn(editor)
        `when`(sharedPreferences.edit()).thenReturn(editor)
        `when`(sharedPreferences.getString(eq("device_uuid"), any())).thenReturn(null)
        `when`(context.getSharedPreferences(anyString(), anyInt())).thenReturn(sharedPreferences)
        val uuid = DeviceUuidFactory(context).uuid
        verify(editor).putString("device_uuid", uuid)
    }

    @Test
    fun uuidSavedInPreferences() {
        val savedUuid = "99111a0f-ca5d-4de1-913a-daba905c53b2"
        val context = mock<Context>()
        val sharedPreferences = mock<SharedPreferences>()
        `when`(sharedPreferences.getString(eq("device_uuid"), any())).thenReturn(savedUuid)
        `when`(context.getSharedPreferences(anyString(), anyInt())).thenReturn(sharedPreferences)
        assertEquals(savedUuid, DeviceUuidFactory(context).uuid)
    }
}