/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

import android.content.Context
import android.content.res.AssetManager
import org.junit.Test
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.*
import java.io.ByteArrayInputStream

class IOUtilsTest {
    /**
     * Verify that IOUtils.readAsset() closes the AssetInputStream.
     */
    @Test
    fun testReadAssetClosesStream() {
        // Setup

        val stream = spy(ByteArrayInputStream("Hello".toByteArray()))

        val assetManager = mock(AssetManager::class.java)
        `when`(assetManager.open(ArgumentMatchers.anyString())).thenReturn(stream)

        val context = mock(Context::class.java)
        `when`(context.assets).thenReturn(assetManager)

        // Now use our mock classes to call IOUtils.close()

        IOUtils.readAsset(context, "test.txt")

        // Verify that the stream was opened and closed

        verify(assetManager).open("test.txt")
        verify(stream).close()
    }
}
