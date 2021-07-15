/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.preparer

import android.content.res.AssetManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.icons.IconRequest
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn

@RunWith(AndroidJUnit4::class)
class TippyTopIconPreparerTest {
    @Test
    fun `WHEN url is github-com THEN resource is added`() {
        val preparer = TippyTopIconPreparer(testContext.assets)

        val request = IconRequest("https://www.github.com")
        assertEquals(0, request.resources.size)

        val preparedRequest = preparer.prepare(testContext, request)
        assertEquals(1, preparedRequest.resources.size)

        val resource = preparedRequest.resources[0]

        assertEquals("https://github.githubassets.com/apple-touch-icon-180x180.png", resource.url)
        assertEquals(IconRequest.Resource.Type.TIPPY_TOP, resource.type)
    }

    @Test
    fun `WHEN url is not in list THEN no resource is added`() {
        val preparer = TippyTopIconPreparer(testContext.assets)

        val request = IconRequest("https://thispageisnotpartofthetippytopylist.org")
        assertEquals(0, request.resources.size)

        val preparedRequest = preparer.prepare(testContext, request)
        assertEquals(0, preparedRequest.resources.size)
    }

    @Test
    fun `WHEN url is not http(s) THEN no resource is added`() {
        val preparer = TippyTopIconPreparer(testContext.assets)

        val request = IconRequest("about://www.github.com")
        assertEquals(0, request.resources.size)

        val preparedRequest = preparer.prepare(testContext, request)
        assertEquals(0, preparedRequest.resources.size)
    }

    @Test
    fun `WHEN list could not be read THEN no resource is added`() {
        val assetManager: AssetManager = mock()
        doReturn("{".toByteArray().inputStream()).`when`(assetManager).open(any())

        val preparer = TippyTopIconPreparer(assetManager)

        val request = IconRequest("https://www.github.com")
        assertEquals(0, request.resources.size)

        val preparedRequest = preparer.prepare(testContext, request)
        assertEquals(0, preparedRequest.resources.size)
    }

    @Test
    fun `WHEN url is Amazon THEN resource is manually added`() {
        val preparer = TippyTopIconPreparer(testContext.assets)

        var request = IconRequest("https://www.amazon.ca")
        assertEquals(0, request.resources.size)

        var preparedRequest = preparer.prepare(testContext, request)
        assertEquals(1, preparedRequest.resources.size)

        var resource = preparedRequest.resources[0]

        assertEquals("https://images-na.ssl-images-amazon.com/images/G/15/anywhere/a_smile_196x196._CB368246733_.png", resource.url)
        assertEquals(IconRequest.Resource.Type.TIPPY_TOP, resource.type)

        request = IconRequest("https://www.amazon.cn")
        assertEquals(0, request.resources.size)

        preparedRequest = preparer.prepare(testContext, request)
        assertEquals(1, preparedRequest.resources.size)

        resource = preparedRequest.resources[0]

        assertEquals("https://images-cn.ssl-images-amazon.com/images/G/28/anywhere/a_smile_196x196._CB368246750_.png", resource.url)
        assertEquals(IconRequest.Resource.Type.TIPPY_TOP, resource.type)

        request = IconRequest("https://www.amazon.co.jp")
        assertEquals(0, request.resources.size)

        preparedRequest = preparer.prepare(testContext, request)
        assertEquals(1, preparedRequest.resources.size)

        resource = preparedRequest.resources[0]

        assertEquals("https://images-fe.ssl-images-amazon.com/images/G/09/anywhere/a_smile_196x196._CB368246755_.png", resource.url)
        assertEquals(IconRequest.Resource.Type.TIPPY_TOP, resource.type)

        request = IconRequest("https://www.amazon.co.uk")
        assertEquals(0, request.resources.size)

        preparedRequest = preparer.prepare(testContext, request)
        assertEquals(1, preparedRequest.resources.size)

        resource = preparedRequest.resources[0]

        assertEquals("https://images-eu.ssl-images-amazon.com/images/G/02/anywhere/a_smile_196x196._CB368246590_.png", resource.url)
        assertEquals(IconRequest.Resource.Type.TIPPY_TOP, resource.type)

        request = IconRequest("https://www.amazon.com")
        assertEquals(0, request.resources.size)

        preparedRequest = preparer.prepare(testContext, request)
        assertEquals(1, preparedRequest.resources.size)

        resource = preparedRequest.resources[0]

        assertEquals("https://images-na.ssl-images-amazon.com/images/G/01/anywhere/a_smile_196x196._CB368246573_.png", resource.url)
        assertEquals(IconRequest.Resource.Type.TIPPY_TOP, resource.type)

        request = IconRequest("https://www.amazon.com.au")
        assertEquals(0, request.resources.size)

        preparedRequest = preparer.prepare(testContext, request)
        assertEquals(1, preparedRequest.resources.size)

        resource = preparedRequest.resources[0]

        assertEquals("https://images-na.ssl-images-amazon.com/images/G/35/anywhere/a_smile_196x196._CB368246314_.png", resource.url)
        assertEquals(IconRequest.Resource.Type.TIPPY_TOP, resource.type)

        request = IconRequest("https://www.amazon.com.br")
        assertEquals(0, request.resources.size)

        preparedRequest = preparer.prepare(testContext, request)
        assertEquals(1, preparedRequest.resources.size)

        resource = preparedRequest.resources[0]

        assertEquals("https://images-na.ssl-images-amazon.com/images/G/32/anywhere/a_smile_196x196._CB368246376_.png", resource.url)
        assertEquals(IconRequest.Resource.Type.TIPPY_TOP, resource.type)

        request = IconRequest("https://www.amazon.com.mx")
        assertEquals(0, request.resources.size)

        preparedRequest = preparer.prepare(testContext, request)
        assertEquals(1, preparedRequest.resources.size)

        resource = preparedRequest.resources[0]

        assertEquals("https://images-na.ssl-images-amazon.com/images/G/33/anywhere/a_smile_196x196._CB368246395_.png", resource.url)
        assertEquals(IconRequest.Resource.Type.TIPPY_TOP, resource.type)

        request = IconRequest("https://www.amazon.de")
        assertEquals(0, request.resources.size)

        preparedRequest = preparer.prepare(testContext, request)
        assertEquals(1, preparedRequest.resources.size)

        resource = preparedRequest.resources[0]

        assertEquals("https://images-eu.ssl-images-amazon.com/images/G/03/anywhere/a_smile_196x196._CB368246539_.png", resource.url)
        assertEquals(IconRequest.Resource.Type.TIPPY_TOP, resource.type)

        request = IconRequest("https://www.amazon.es")
        assertEquals(0, request.resources.size)

        preparedRequest = preparer.prepare(testContext, request)
        assertEquals(1, preparedRequest.resources.size)

        resource = preparedRequest.resources[0]

        assertEquals("https://images-na.ssl-images-amazon.com/images/G/30/anywhere/a_smile_196x196._CB368246671_.png", resource.url)
        assertEquals(IconRequest.Resource.Type.TIPPY_TOP, resource.type)

        request = IconRequest("https://www.amazon.fr")
        assertEquals(0, request.resources.size)

        preparedRequest = preparer.prepare(testContext, request)
        assertEquals(1, preparedRequest.resources.size)

        resource = preparedRequest.resources[0]

        assertEquals("https://images-eu.ssl-images-amazon.com/images/G/08/anywhere/a_smile_196x196._CB368246545_.png", resource.url)
        assertEquals(IconRequest.Resource.Type.TIPPY_TOP, resource.type)

        request = IconRequest("https://www.amazon.in")
        assertEquals(0, request.resources.size)

        preparedRequest = preparer.prepare(testContext, request)
        assertEquals(1, preparedRequest.resources.size)

        resource = preparedRequest.resources[0]

        assertEquals("https://images-eu.ssl-images-amazon.com/images/G/31/anywhere/a_smile_196x196._CB368246681_.png", resource.url)
        assertEquals(IconRequest.Resource.Type.TIPPY_TOP, resource.type)

        request = IconRequest("https://www.amazon.it")
        assertEquals(0, request.resources.size)

        preparedRequest = preparer.prepare(testContext, request)
        assertEquals(1, preparedRequest.resources.size)

        resource = preparedRequest.resources[0]

        assertEquals("https://images-eu.ssl-images-amazon.com/images/G/29/anywhere/a_smile_196x196._CB368246716_.png", resource.url)
        assertEquals(IconRequest.Resource.Type.TIPPY_TOP, resource.type)
    }

    @Test
    fun `WHEN url is Wikipedia THEN prefix is ignored`() {
        val preparer = TippyTopIconPreparer(testContext.assets)

        var request = IconRequest("https://www.wikipedia.org")
        assertEquals(0, request.resources.size)

        var preparedRequest = preparer.prepare(testContext, request)
        assertEquals(1, preparedRequest.resources.size)

        var resource = preparedRequest.resources[0]

        assertEquals("https://www.wikipedia.org/static/apple-touch/wikipedia.png", resource.url)
        assertEquals(IconRequest.Resource.Type.TIPPY_TOP, resource.type)

        request = IconRequest("https://en.wikipedia.org")
        assertEquals(0, request.resources.size)

        preparedRequest = preparer.prepare(testContext, request)
        assertEquals(1, preparedRequest.resources.size)

        resource = preparedRequest.resources[0]

        assertEquals("https://www.wikipedia.org/static/apple-touch/wikipedia.png", resource.url)
        assertEquals(IconRequest.Resource.Type.TIPPY_TOP, resource.type)

        request = IconRequest("https://de.wikipedia.org")
        assertEquals(0, request.resources.size)

        preparedRequest = preparer.prepare(testContext, request)
        assertEquals(1, preparedRequest.resources.size)

        resource = preparedRequest.resources[0]

        assertEquals("https://www.wikipedia.org/static/apple-touch/wikipedia.png", resource.url)
        assertEquals(IconRequest.Resource.Type.TIPPY_TOP, resource.type)

        request = IconRequest("https://de.m.wikipedia.org")
        assertEquals(0, request.resources.size)

        preparedRequest = preparer.prepare(testContext, request)
        assertEquals(1, preparedRequest.resources.size)

        resource = preparedRequest.resources[0]

        assertEquals("https://www.wikipedia.org/static/apple-touch/wikipedia.png", resource.url)
        assertEquals(IconRequest.Resource.Type.TIPPY_TOP, resource.type)

        request = IconRequest("https://abc.wikipedia.org.com")
        assertEquals(0, request.resources.size)

        preparedRequest = preparer.prepare(testContext, request)
        assertEquals(0, preparedRequest.resources.size)
    }
}
