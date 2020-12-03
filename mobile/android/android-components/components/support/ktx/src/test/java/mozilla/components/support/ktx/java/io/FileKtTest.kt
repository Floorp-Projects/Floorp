/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.java.io

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File
import java.util.UUID

@RunWith(AndroidJUnit4::class)
class FileKtTest {

    @Test
    fun truncateDirectory() {
        val root = File(System.getProperty("java.io.tmpdir"), UUID.randomUUID().toString())
        assertTrue(root.mkdir())

        val file1 = File(root, "file1")
        assertTrue(file1.createNewFile())

        val file2 = File(root, "file2")
        assertTrue(file2.createNewFile())

        val dir1 = File(root, "dir1")
        assertTrue(dir1.mkdir())

        val dir2 = File(root, "dir2")
        assertTrue(dir2.mkdir())

        val file3 = File(dir2, "file3")
        file3.createNewFile()

        assertEquals(4, root.listFiles()?.size)

        root.truncateDirectory()

        assertEquals(0, root.listFiles()?.size)
    }
}
