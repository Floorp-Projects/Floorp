/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.processor

import android.os.Build
import androidx.core.graphics.createBitmap
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest
import mozilla.components.browser.icons.IconRequest.Resource.Type.MANIFEST_ICON
import mozilla.components.support.test.mock
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.util.ReflectionHelpers.setStaticField
import kotlin.reflect.jvm.javaField

@RunWith(AndroidJUnit4::class)
class AdaptiveIconProcessorTest {

    @Before
    fun setup() {
        setSdkInt(0)
    }

    @After
    fun teardown() = setSdkInt(0)

    @Test
    fun `process returns non-maskable icons on legacy devices`() {
        val icon = Icon(mock(), source = Icon.Source.GENERATOR)

        assertEquals(
            icon,
            AdaptiveIconProcessor().process(mock(), mock(), mock(), icon, mock()),
        )
    }

    @Test
    fun `process adds padding to legacy icons`() {
        setSdkInt(Build.VERSION_CODES.O)
        val bitmap = spy(createBitmap(128, 128))

        val icon = AdaptiveIconProcessor().process(
            mock(),
            mock(),
            IconRequest.Resource("", MANIFEST_ICON, maskable = false),
            Icon(bitmap, source = Icon.Source.DISK),
            mock(),
        )

        assertEquals(228, icon.bitmap.width)
        assertEquals(228, icon.bitmap.height)

        assertEquals(Icon.Source.DISK, icon.source)
        assertTrue(icon.maskable)
        verify(bitmap).recycle()
    }

    @Test
    fun `process adjusts the size of maskable icons`() {
        val bitmap = createBitmap(256, 256)

        val icon = AdaptiveIconProcessor().process(
            mock(),
            mock(),
            IconRequest.Resource("", MANIFEST_ICON, maskable = true),
            Icon(bitmap, source = Icon.Source.INLINE),
            mock(),
        )

        assertEquals(334, icon.bitmap.width)
        assertEquals(334, icon.bitmap.height)

        assertEquals(Icon.Source.INLINE, icon.source)
        assertTrue(icon.maskable)
    }

    private fun setSdkInt(sdkVersion: Int) {
        setStaticField(Build.VERSION::SDK_INT.javaField, sdkVersion)
    }
}
