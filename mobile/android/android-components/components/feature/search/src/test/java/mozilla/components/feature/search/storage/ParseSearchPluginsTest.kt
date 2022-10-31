/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.storage

import android.text.TextUtils
import mozilla.components.browser.state.search.SearchEngine
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.ParameterizedRobolectricTestRunner
import java.io.File
import java.io.FileInputStream

@RunWith(ParameterizedRobolectricTestRunner::class)
class ParseSearchPluginsTest(private val searchEngineIdentifier: String) {

    @Test
    @Throws(Exception::class)
    fun parser() {
        val stream = FileInputStream(File(basePath, searchEngineIdentifier))
        val searchEngine = SearchEngineReader(type = SearchEngine.Type.BUNDLED)
            .loadStream(searchEngineIdentifier, stream)

        assertEquals(searchEngineIdentifier, searchEngine.id)

        assertNotNull(searchEngine.name)
        assertFalse(TextUtils.isEmpty(searchEngine.name))

        assertNotNull(searchEngine.icon)

        val searchUrl = searchEngine.resultUrls
        assertTrue(searchUrl.isNotEmpty())

        stream.close()
    }

    companion object {
        @JvmStatic
        @ParameterizedRobolectricTestRunner.Parameters(name = "{0}")
        fun searchPlugins(): Collection<Array<Any>> = basePath.list().orEmpty()
            .mapNotNull { it as Any }
            .map { arrayOf(it) }
            .apply { if (isEmpty()) { throw IllegalStateException("No search plugins found.") } }

        private val basePath: File
            get() = ParseSearchPluginsTest::class.java.classLoader!!
                .getResource("searchplugins").file
                .let { File(it) }
    }
}
