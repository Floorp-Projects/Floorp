/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.intent

import android.content.Intent
import android.net.Uri
import junit.framework.TestCase
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class ExternalDeepLinkIntentProcessorTest : TestCase() {

    @Test
    fun `GIVEN a deeplink intent WHEN processing the intent THEN add the extra flags`() {
        val processor = ExternalDeepLinkIntentProcessor()
        val uri = Uri.parse(BuildConfig.DEEP_LINK_SCHEME + "://settings_wallpapers")
        val intent = Intent("", uri)

        val result = processor.process(intent)

        assertTrue(result)
        assertTrue((intent.flags and (Intent.FLAG_ACTIVITY_NEW_TASK) != 0))
        assertTrue((intent.flags and (Intent.FLAG_ACTIVITY_CLEAR_TOP) != 0))
    }

    @Test
    fun `GIVEN a non-deeplink intent WHEN processing the intent THEN do not add the extra flags`() {
        val processor = ExternalDeepLinkIntentProcessor()
        val intent = Intent("")

        val result = processor.process(intent)

        assertFalse(result)
        assertFalse((intent.flags and (Intent.FLAG_ACTIVITY_NEW_TASK) != 0))
        assertFalse((intent.flags and (Intent.FLAG_ACTIVITY_CLEAR_TOP) != 0))
    }
}
