/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.graphics.Color
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class ColorUtilsTest {

    @Test
    fun getReadableTextColor() {
        assertEquals(Color.BLACK.toLong(), ColorUtils.getReadableTextColor(Color.WHITE).toLong())
        assertEquals(Color.WHITE.toLong(), ColorUtils.getReadableTextColor(Color.BLACK).toLong())

        // Slack
        assertEquals(Color.BLACK.toLong(), ColorUtils.getReadableTextColor(-0x90b14).toLong())

        // Google+
        assertEquals(Color.WHITE.toLong(), ColorUtils.getReadableTextColor(-0x24bbc9).toLong())

        // Telegram
        assertEquals(Color.WHITE.toLong(), ColorUtils.getReadableTextColor(-0xad825d).toLong())

        // IRCCloud
        assertEquals(Color.BLACK.toLong(), ColorUtils.getReadableTextColor(-0xd0804).toLong())

        // Yahnac
        assertEquals(Color.WHITE.toLong(), ColorUtils.getReadableTextColor(-0xa8400).toLong())
    }

    @Test
    fun isDark() {
        assertTrue(ColorUtils.isDark(Color.BLACK))
        assertFalse(ColorUtils.isDark(Color.WHITE))
        assertFalse(ColorUtils.isDark(Color.TRANSPARENT))
        assertFalse(ColorUtils.isDark(50 shl 24 /* Alpha 50 */))
        assertFalse(ColorUtils.isDark(127 shl 24 /* Alpha 127 */))
        assertTrue(ColorUtils.isDark(128 shl 24 /* Alpha 128 */))
        assertTrue(ColorUtils.isDark(255 shl 24 /* Alpha 255 */))
    }
}
