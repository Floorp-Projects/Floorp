/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.webnotifications

import android.content.Intent
import android.os.Parcelable
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.Engine
import mozilla.components.support.test.mock
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class WebNotificationIntentProcessorTest {
    private val engine: Engine = mock()
    private val processor = WebNotificationIntentProcessor(engine)

    @Test
    fun `GIVEN an Intent WHEN it contains a parcelable with our private key THEN processing is successful`() {
        val notification = mock<Parcelable>()
        val intent = Intent().putExtra(NativeNotificationBridge.EXTRA_ON_CLICK, notification)

        val result = processor.process(intent)

        assertTrue(result)
    }

    @Test
    fun `GIVEN an Intent WHEN it does not contain a parcelable with our private key THEN fail at processing the intent`() {
        val intent = Intent()

        val result = processor.process(intent)

        assertFalse(result)
    }

    @Test
    fun `GIVEN an Intent WHEN it contains a parcelable with our private key THEN delegate the engine to handle it`() {
        val notification = mock<Parcelable>()
        val intent = Intent().putExtra(NativeNotificationBridge.EXTRA_ON_CLICK, notification)

        processor.process(intent)

        verify(engine).handleWebNotificationClick(notification)
    }
}
