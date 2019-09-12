package org.mozilla.fenix.ext

import mozilla.components.support.test.robolectric.testContext
import android.widget.ImageView
import kotlinx.coroutines.ObsoleteCoroutinesApi
import io.mockk.verify
import io.mockk.spyk
import mozilla.components.browser.icons.IconRequest
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import org.junit.Test
import org.mozilla.fenix.TestApplication
import mozilla.components.browser.icons.BrowserIcons
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.junit.runner.RunWith

@ObsoleteCoroutinesApi
@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)

class BrowserIconsTest {
    @Test
    fun loadIntoViewTest() {
        val imageView = spyk(ImageView(testContext))
        val icons = spyk(BrowserIcons(testContext, httpClient = HttpURLConnectionClient()))
        val myUrl = "https://mozilla.com"
        val request = spyk(IconRequest(url = myUrl))
        icons.loadIntoView(imageView, myUrl)
        verify { icons.loadIntoView(imageView, request) }
    }
}
