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
import java.util.ArrayList
import java.util.UUID

@RunWith(ParameterizedRobolectricTestRunner::class)
class ParseSearchPluginsTest(private val searchPluginPath: String, private val searchEngineIdentifier: String) {

    @Test
    @Throws(Exception::class)
    fun testParser() {
        val stream = FileInputStream(searchPluginPath)
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
        @ParameterizedRobolectricTestRunner.Parameters(name = "{1}")
        fun searchPlugins(): Collection<Array<Any>> {
            val searchPlugins = ArrayList<Array<Any>>()
            collectSearchPlugins(searchPlugins, basePath)
            return searchPlugins
        }

        private fun collectSearchPlugins(searchPlugins: MutableCollection<Array<Any>>, path: File) {
            if (!path.isDirectory) {
                throw AssertionError("Not a directory: " + path.absolutePath)
            }

            val entries = path.list()
                    ?: throw AssertionError("No files in directory " + path.absolutePath)

            for (entry in entries) {
                val file = File(path, entry)

                if (file.isDirectory) {
                    collectSearchPlugins(searchPlugins, file)
                } else if (entry.endsWith(".xml")) {
                    searchPlugins.add(arrayOf(file.absolutePath, entry.substring(0, entry.length - 4)))
                }
            }
        }

        private val basePath: File
            get() {
                val classLoader = ParseSearchPluginsTest::class.java.classLoader
                return File(classLoader.getResource("search").file)
            }
    }
}
