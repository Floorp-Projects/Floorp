/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.util

import android.util.AtomicFile
import androidx.core.util.writeText
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.json.JSONException
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.any
import org.mockito.Mockito.doThrow
import org.mockito.Mockito.verify
import java.io.File
import java.io.FileNotFoundException
import java.io.IOException

@RunWith(AndroidJUnit4::class)
class AtomicFileTest {

    @Test
    fun `writeString - Fails write on IOException`() {
        val mockedFile: AtomicFile = mock()
        doThrow(IOException::class.java).`when`(mockedFile).startWrite()

        val result = mockedFile.writeString { "file_content" }

        assertFalse(result)
        verify(mockedFile).failWrite(any())
    }

    @Test
    fun `writeString - Fails write on JSONException`() {
        val mockedFile: AtomicFile = mock()
        whenever(mockedFile.startWrite()).thenAnswer {
            throw JSONException("")
        }

        val result = mockedFile.writeString { "file_content" }

        assertFalse(result)
        verify(mockedFile).failWrite(any())
    }

    @Test
    fun `writeString - writes the content of the file`() {
        val tempFile = File.createTempFile("temp", ".tmp")
        val atomicFile = AtomicFile(tempFile)
        atomicFile.writeString { "file_content" }

        val result = atomicFile.writeString { "file_content" }
        assertTrue(result)
    }

    @Test
    fun `readAndDeserialize - Returns the content of the file`() {
        val tempFile = File.createTempFile("temp", ".tmp")
        val atomicFile = AtomicFile(tempFile)
        atomicFile.writeText("file_content")

        val fileContent = atomicFile.readAndDeserialize { it }
        assertNotNull(fileContent)
        assertEquals("file_content", fileContent)
    }

    @Test
    fun `readAndDeserialize - Returns null on FileNotFoundException`() {
        val mockedFile: AtomicFile = mock()
        doThrow(FileNotFoundException::class.java).`when`(mockedFile).openRead()

        val content = mockedFile.readAndDeserialize { it }
        assertNull(content)
    }

    @Test
    fun `readAndDeserialize - Returns null on JSONException`() {
        val mockedFile: AtomicFile = mock()
        whenever(mockedFile.openRead()).thenAnswer {
            throw JSONException("")
        }

        val content = mockedFile.readAndDeserialize { it }
        assertNull(content)
    }
}
