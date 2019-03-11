/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.ext

import android.util.AtomicFile
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.storage.SnapshotSerializer
import mozilla.components.browser.session.storage.getFileForEngine
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertNull
import org.junit.Test
import org.mockito.Mockito
import org.robolectric.RuntimeEnvironment
import java.io.FileNotFoundException
import java.io.IOException

class AtomicFileKtTest {
    @Test
    fun `writeSnapshot - Fails write on IOException`() {
        val file: AtomicFile = mock()
        Mockito.doThrow(IOException::class.java).`when`(file).startWrite()

        val snapshot = SessionManager.Snapshot(
            sessions = listOf(
                SessionManager.Snapshot.Item(Session("http://mozilla.org"))
            ),
            selectedSessionIndex = 0
        )

        file.writeSnapshot(snapshot, SnapshotSerializer())

        Mockito.verify(file).failWrite(any())
    }

    @Test
    fun `readSnapshot - Returns null on FileNotFoundException`() {
        val file: AtomicFile = mock()
        Mockito.doThrow(FileNotFoundException::class.java).`when`(file).openRead()

        val snapshot = file.readSnapshot(engine = mock(), serializer = SnapshotSerializer())
        assertNull(snapshot)
    }

    @Test
    fun `readSnapshot - Returns null on corrupt JSON`() {
        val file = getFileForEngine(RuntimeEnvironment.application, engine = mock())

        val stream = file.startWrite()
        stream.bufferedWriter().write("{ name: 'Foo")
        file.finishWrite(stream)

        val snapshot = file.readSnapshot(engine = mock(), serializer = SnapshotSerializer())
        assertNull(snapshot)
    }
}