/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.search

import android.text.TextUtils
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.ParameterizedRobolectricTestRunner
import java.io.File
import java.io.FileInputStream
import java.util.UUID

@RunWith(ParameterizedRobolectricTestRunner::class)
class ParseSearchPluginsTest(private val searchEngineIdentifier: String) {

    @Test
    @Throws(Exception::class)
    fun parser() {
        val stream = FileInputStream(File(basePath, searchEngineIdentifier))
        val searchEngine = SearchEngineParser().load(searchEngineIdentifier, stream)
        assertEquals(searchEngineIdentifier, searchEngine.identifier)

        assertNotNull(searchEngine.name)
        assertFalse(TextUtils.isEmpty(searchEngine.name))

        assertNotNull(searchEngine.icon)

        val searchTerm = UUID.randomUUID().toString()
        val searchUrl = searchEngine.buildSearchUrl(searchTerm)

        assertNotNull(searchUrl)
        assertFalse(TextUtils.isEmpty(searchUrl))
        assertTrue(searchUrl.contains(searchTerm))

        stream.close()
    }

    companion object {
        @JvmStatic
        @ParameterizedRobolectricTestRunner.Parameters(name = "{0}")
        fun searchPlugins(): Collection<Array<Any>> = basePath.list().orEmpty()
            .mapNotNull { it as Any }
            .map { arrayOf(it) }

        private val basePath: File
            get() = ParseSearchPluginsTest::class.java.classLoader!!
                .getResource("searchplugins").file
                .let { File(it) }
    }
}
