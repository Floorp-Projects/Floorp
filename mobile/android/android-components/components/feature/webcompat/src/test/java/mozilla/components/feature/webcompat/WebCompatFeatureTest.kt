/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.webcompat

import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.webextensions.WebExtensionController
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

class WebCompatFeatureTest {

    @Before
    fun setup() {
        WebExtensionController.installedExtensions.clear()
    }

    @Test
    fun `installs the webextension`() {
        val engine: Engine = mock()

        val webcompatFeature = spy(WebCompatFeature)
        webcompatFeature.install(engine)

        val onSuccess = argumentCaptor<((WebExtension) -> Unit)>()
        val onError = argumentCaptor<((Throwable) -> Unit)>()
        verify(engine, times(1)).installBuiltInWebExtension(
            eq(WebCompatFeature.WEBCOMPAT_EXTENSION_ID),
            eq(WebCompatFeature.WEBCOMPAT_EXTENSION_URL),
            onSuccess.capture(),
            onError.capture(),
        )
    }
}
