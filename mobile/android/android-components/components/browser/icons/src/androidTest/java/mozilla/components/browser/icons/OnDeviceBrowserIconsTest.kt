/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.icons.generator.IconGenerator
import mozilla.components.concept.engine.manifest.Size
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test

class OnDeviceBrowserIconsTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    @OptIn(ExperimentalCoroutinesApi::class)
    @Test
    fun dataUriLoad() = runTest {
        val request = IconRequest(
            url = "https://www.mozilla.org",
            size = IconRequest.Size.DEFAULT,
            resources = listOf(
                IconRequest.Resource(
                    url = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAGQAAABkCAMAAABHPGVmAAAA21BMVE" +
                        "UAAAADAwMREREhISExMTFBQUFRUVFhYWFxcXGBgYFQUlBgYmCQkJA/Gxt8Hx9BQEFQUVBQj0JenE+A" +
                        "gICgoKAgISA7Li5cODhQUFBgYGBudmx9hXywsLAwMDA5OlJISWF5eWiBgX+Qj5Cgar+vo7bAwMBAQE" +
                        "A7PnxJTIpycm7Cwj2YmIagn6CujMG/t8PPz89wcHCAgH+Sko2foJ+vr6+/v7/f399fX19vb2+mdFmd" +
                        "ioCfn5+Xy8u11dXv7+9/f3+lh3anm5Wk2dnD5OT8/PyPj4/e3t/u7u7///9PuU9UAAAAq0lEQVR4Ae" +
                        "3YIQ7CQBCF4T4ou0uQaCxX4P5XAtVicA8zadIsTabh/9W6TzzRdDQ4bfY6DNsFsiYQEK3uFKSgo2OT" +
                        "JAgIiL7K5Sff88mvxibpEBCQqzt3NLkWxCZJEBAQ3Ra/2LNfdf//8SAgILpzufsjBATkGdQ6kquOTZ" +
                        "IgICB69NzmuNztDAEBkauLTU6uBDVXHJtkQUBAqivu7V5ObnZjUHGjY5MkCAjIBymjUnvFUjKoAAAA" +
                        "AElFTkSuQmCC",
                    sizes = listOf(Size(64, 64)),
                    mimeType = "image/png",
                    type = IconRequest.Resource.Type.FAVICON,
                ),
            ),
        )

        val icon = BrowserIcons(
            context,
            httpClient = object : Client() {
                override fun fetch(request: Request): Response {
                    @Suppress("TooGenericExceptionThrown")
                    throw RuntimeException("Client execution not expected")
                }
            },
            generator = object : IconGenerator {
                override fun generate(context: Context, request: IconRequest): Icon {
                    @Suppress("TooGenericExceptionThrown")
                    throw RuntimeException("Generator execution not expected")
                }
            },
        ).loadIcon(request).await()

        assertNotNull(icon)

        val bitmap = icon.bitmap
        assertEquals(100, bitmap.width)
        assertEquals(100, bitmap.height)
    }
}
