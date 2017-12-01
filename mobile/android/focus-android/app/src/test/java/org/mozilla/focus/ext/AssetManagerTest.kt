/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.ext

import android.content.Context
import android.content.res.AssetManager
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito
import org.robolectric.RobolectricTestRunner
import java.io.ByteArrayInputStream

@RunWith(RobolectricTestRunner::class)
class AssetManagerTest {
    /**
     * Verify that AssetManager.readJSONObject() closes its stream.
     */
    @Test
    fun testReadJSONObjectClosesStream() {
        // Setup

        val stream = Mockito.spy(ByteArrayInputStream("{}".toByteArray()))

        val assetManager = Mockito.mock(AssetManager::class.java)
        Mockito.`when`(assetManager.open(ArgumentMatchers.anyString())).thenReturn(stream)

        val context = Mockito.mock(Context::class.java)
        Mockito.`when`(context.assets).thenReturn(assetManager)

        // Now use our mock classes to call readJSONObject()

        context.assets.readJSONObject("test.txt")

        // Verify that the stream was opened and closed

        Mockito.verify(assetManager).open("test.txt")
        Mockito.verify(stream).close()
    }

    /**
     * The stream returned by the AssetManager will be read and converted into a JSONObject instance.
     */
    @Test
    fun testStreamsIsTransformedIntoJSONObject() {
        // Setup

        val stream = Mockito.spy(ByteArrayInputStream("{'firstName': 'John', 'lastName': 'Smith'}".toByteArray()))

        val assetManager = Mockito.mock(AssetManager::class.java)
        Mockito.`when`(assetManager.open(ArgumentMatchers.anyString())).thenReturn(stream)

        val context = Mockito.mock(Context::class.java)
        Mockito.`when`(context.assets).thenReturn(assetManager)

        // Now read the stream into an JSONObject

        val data = context.assets.readJSONObject("test.txt")

        // Assert that the JSONObject was constructed correctly.

        Mockito.verify(assetManager).open("test.txt")

        assertNotNull(data)
        assertEquals(2, data.length())
        assertTrue(data.has("firstName"))
        assertTrue(data.has("lastName"))
        assertEquals("John", data.getString("firstName"))
        assertEquals("Smith", data.getString("lastName"))
    }
}
