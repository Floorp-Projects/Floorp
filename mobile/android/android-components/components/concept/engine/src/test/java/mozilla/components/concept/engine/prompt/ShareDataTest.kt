/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.prompt

import android.os.Bundle
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class ShareDataTest {

    @Test
    fun `Create share data`() {
        val onlyTitle = ShareData(title = "Title")
        assertEquals("Title", onlyTitle.title)

        val onlyText = ShareData(text = "Text")
        assertEquals("Text", onlyText.text)

        val onlyUrl = ShareData(url = "https://mozilla.org")
        assertEquals("https://mozilla.org", onlyUrl.url)
    }

    @Test
    fun `Save to bundle`() {
        val noText = ShareData(title = "Title", url = "https://mozilla.org")
        val noUrl = ShareData(title = "Title", text = "Text")
        val bundle = Bundle().apply {
            putParcelable("noText", noText)
            putParcelable("noUrl", noUrl)
        }
        assertEquals(noText, bundle.getParcelable<ShareData>("noText"))
        assertEquals(noUrl, bundle.getParcelable<ShareData>("noUrl"))
    }
}
