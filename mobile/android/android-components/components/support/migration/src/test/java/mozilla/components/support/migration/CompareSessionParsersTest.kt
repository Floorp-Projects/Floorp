/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import mozilla.components.browser.session.storage.RecoverableBrowserState
import mozilla.components.support.migration.session.InMemorySessionStoreParser
import mozilla.components.support.migration.session.StreamingSessionStoreParser
import org.json.JSONException
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.ParameterizedRobolectricTestRunner
import java.io.File
import java.io.IOException

/**
 * This tests tries parsing all session stores in the /sessions/ folder with
 * [InMemorySessionStoreParser] and [StreamingSessionStoreParser] and then asserts that they both
 * yield the same results.
 */
@RunWith(ParameterizedRobolectricTestRunner::class)
class CompareSessionParsersTest(
    private val sessionStore: File,
    private val resource: String
) {
    @Test
    fun compareParsers() {
        println("Parsing $resource")

        val snapshot1 = tryInMemoryParser(sessionStore)
        val snapshot2 = tryStreamingParser(sessionStore)

        if (snapshot1 == null || snapshot2 == null) {
            // If one of the parsers returned null then we expect both values to be null
            assertNull(snapshot1)
            assertNull(snapshot2)
            return
        }

        assertEquals(snapshot1.tabs.size, snapshot2.tabs.size)
        assertEquals(snapshot1.selectedTabIndex(), snapshot2.selectedTabIndex())

        snapshot1.tabs.forEachIndexed { index, item1 ->
            val item2 = snapshot2.tabs[index]

            assertEquals(item1.state.url, item2.state.url)
            assertEquals(item1.state.title, item2.state.title)
        }
    }

    private fun tryInMemoryParser(file: File): RecoverableBrowserState? {
        return try {
            InMemorySessionStoreParser.parse(file).value
        } catch (e: JSONException) {
            null
        } catch (e: IOException) {
            null
        }
    }

    private fun tryStreamingParser(file: File): RecoverableBrowserState? {
        return try {
            StreamingSessionStoreParser.parse(file).value
        } catch (e: JSONException) {
            null
        } catch (e: IOException) {
            null
        }
    }

    companion object {
        @JvmStatic
        @ParameterizedRobolectricTestRunner.Parameters(name = "{1}")
        fun sessionStores(): Collection<Array<Any>> = basePath.list().orEmpty()
            .mapNotNull { it as String }
            .map { File(basePath, it) }
            .flatMap {
                listOf(
                    File(it, "sessionstore.js"),
                    File(it, "sessionstore.bak")
                )
            }
            .filter { it.exists() }
            .map { arrayOf<Any>(it, (it.parentFile?.name ?: "") + File.separator + it.name) }

        private val basePath: File
            get() = CompareSessionParsersTest::class.java.classLoader!!
                .getResource("sessions").file
                .let { File(it) }
    }
}

private fun RecoverableBrowserState.selectedTabIndex(): Int {
    val tab = tabs.firstOrNull { tab -> tab.state.id == selectedTabId } ?: return -1
    return tabs.indexOf(tab)
}
