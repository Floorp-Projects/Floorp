/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash

import android.content.ComponentName
import android.content.Intent
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class NativeCodeCrashTest {

    @Test
    fun `Creating NativeCodeCrash object from sample GeckoView intent`() {
        val intent = Intent("org.mozilla.gecko.ACTION_CRASHED")
        intent.component = ComponentName(
            "org.mozilla.samples.browser",
            "mozilla.components.lib.crash.handler.CrashHandlerService",
        )
        intent.putExtra("uuid", "afc91225-93d7-4328-b3eb-d26ad5af4d86")
        intent.putExtra(
            "minidumpPath",
            "/data/data/org.mozilla.samples.browser/files/mozilla/Crash Reports/pending/3ba5f665-8422-dc8e-a88e-fc65c081d304.dmp",
        )
        intent.putExtra("processType", "FOREGROUND_CHILD")
        intent.putExtra(
            "extrasPath",
            "/data/data/org.mozilla.samples.browser/files/mozilla/Crash Reports/pending/3ba5f665-8422-dc8e-a88e-fc65c081d304.extra",
        )
        intent.putExtra("minidumpSuccess", true)

        val crash = Crash.NativeCodeCrash.fromBundle(intent.extras!!)

        assertEquals(
            "afc91225-93d7-4328-b3eb-d26ad5af4d86",
            crash.uuid,
        )
        assertEquals(crash.minidumpSuccess, true)
        assertEquals(crash.isFatal, false)
        assertEquals(crash.processType, Crash.NativeCodeCrash.PROCESS_TYPE_FOREGROUND_CHILD)
        assertEquals(
            "/data/data/org.mozilla.samples.browser/files/mozilla/Crash Reports/pending/3ba5f665-8422-dc8e-a88e-fc65c081d304.dmp",
            crash.minidumpPath,
        )
        assertEquals(
            "/data/data/org.mozilla.samples.browser/files/mozilla/Crash Reports/pending/3ba5f665-8422-dc8e-a88e-fc65c081d304.extra",
            crash.extrasPath,
        )
    }

    @Test
    fun `to and from bundle`() {
        val crash = Crash.NativeCodeCrash(
            0,
            "minidumpPath",
            true,
            "extrasPath",
            Crash.NativeCodeCrash.PROCESS_TYPE_MAIN,
            arrayListOf(),
        )

        val bundle = crash.toBundle()
        val otherCrash = Crash.NativeCodeCrash.fromBundle(bundle)

        assertEquals(crash, otherCrash)
    }
}
