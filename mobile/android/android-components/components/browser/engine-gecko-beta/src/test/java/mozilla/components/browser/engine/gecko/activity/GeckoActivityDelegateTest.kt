/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.activity

import android.app.PendingIntent
import android.content.Intent
import android.content.IntentSender
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.activity.ActivityDelegate
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mozilla.geckoview.GeckoResult
import java.lang.ref.WeakReference

@RunWith(AndroidJUnit4::class)
class GeckoActivityDelegateTest {
    lateinit var pendingIntent: PendingIntent

    @Before
    fun setup() {
        pendingIntent = mock()
        `when`(pendingIntent.intentSender).thenReturn(mock())
    }

    @Test
    fun `onStartActivityForResult is completed successfully`() {
        val delegate: ActivityDelegate = object : ActivityDelegate {
            override fun startIntentSenderForResult(intent: IntentSender, onResult: (Intent?) -> Unit) {
                onResult(mock())
            }
        }

        val geckoActivityDelegate = GeckoActivityDelegate(WeakReference(delegate))
        val result = geckoActivityDelegate.onStartActivityForResult(pendingIntent)

        result.accept {
            assertNotNull(it)
        }
    }

    @Test
    fun `onStartActivityForResult completes exceptionally on null response`() {
        val delegate: ActivityDelegate = object : ActivityDelegate {
            override fun startIntentSenderForResult(intent: IntentSender, onResult: (Intent?) -> Unit) {
                onResult(null)
            }
        }

        val geckoActivityDelegate = GeckoActivityDelegate(WeakReference(delegate))
        val result = geckoActivityDelegate.onStartActivityForResult(pendingIntent)

        result.exceptionally { throwable ->
            assertEquals("Activity for result failed.", throwable.localizedMessage)
            GeckoResult.fromValue(null)
        }
    }

    @Test
    fun `onStartActivityForResult completes exceptionally when there is no object attached to the weak reference`() {
        val geckoActivityDelegate = GeckoActivityDelegate(WeakReference(null))
        val result = geckoActivityDelegate.onStartActivityForResult(pendingIntent)

        result.exceptionally { throwable ->
            assertEquals("Activity for result failed; no delegate attached.", throwable.localizedMessage)
            GeckoResult.fromValue(null)
        }
    }
}
