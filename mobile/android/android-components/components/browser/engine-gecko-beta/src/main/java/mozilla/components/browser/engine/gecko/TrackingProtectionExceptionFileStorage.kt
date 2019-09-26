/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import android.content.Context
import android.util.AtomicFile
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.content.blocking.TrackingProtectionExceptionStorage
import mozilla.components.support.ktx.util.readAndDeserialize
import mozilla.components.support.ktx.util.writeString
import org.mozilla.geckoview.GeckoRuntime
import java.io.File

private const val STORE_FILE_NAME_FORMAT =
    "mozilla_components_tracking_protection_storage_gecko.json"

/**
 * A [TrackingProtectionExceptionStorage] implementation to store tracking protection exceptions.
 */
internal class TrackingProtectionExceptionFileStorage(
    private val context: Context,
    private val runtime: GeckoRuntime
) : TrackingProtectionExceptionStorage {
    private val fileLock = Any()
    internal var scope = CoroutineScope(Dispatchers.IO)

    /**
     * Restore all exceptions from the [STORE_FILE_NAME_FORMAT] file,
     * and provides them to the gecko [runtime].
     */
    override fun restore() {
        scope.launch {
            synchronized(fileLock) {
                getFile(context).readAndDeserialize { json ->
                    if (json.isNotEmpty()) {
                        val exceptionList = runtime.contentBlockingController.ExceptionList(json)
                        runtime.contentBlockingController.restoreExceptionList(exceptionList)
                    }
                }
            }
        }
    }

    override fun contains(session: EngineSession, onResult: (Boolean) -> Unit) {
        val geckoSession = (session as GeckoEngineSession).geckoSession
        runtime.contentBlockingController.checkException(geckoSession).accept {
            if (it != null) {
                onResult(it)
            } else {
                onResult(false)
            }
        }
    }

    override fun fetchAll(onResult: (List<String>) -> Unit) {
        runtime.contentBlockingController.saveExceptionList().accept { exceptionList ->
            val exceptions = if (exceptionList != null) {
                val uris = exceptionList.uris.map { uri ->
                    uri
                }
                uris
            } else {
                emptyList()
            }
            onResult(exceptions)
        }
    }

    override fun add(session: EngineSession) {
        val geckoSession = (session as GeckoEngineSession).geckoSession
        runtime.contentBlockingController.addException(geckoSession)
        persist()
    }

    override fun remove(session: EngineSession) {
        val geckoSession = (session as GeckoEngineSession).geckoSession
        runtime.contentBlockingController.removeException(geckoSession)
        persist()
    }

    override fun removeAll() {
        runtime.contentBlockingController.clearExceptionList()
        removeFileFromDisk(context)
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getFile(context: Context): AtomicFile {
        return AtomicFile(
            File(
                context.filesDir,
                STORE_FILE_NAME_FORMAT
            )
        )
    }

    /**
     * Take all the exception from the gecko [runtime] and saves them into the
     * [STORE_FILE_NAME_FORMAT] file.
     */
    private fun persist() {
        runtime.contentBlockingController.saveExceptionList().accept { exceptionList ->
            if (exceptionList != null) {
                scope.launch {
                    synchronized(fileLock) {
                        getFile(context).writeString {
                            exceptionList.toJson().toString()
                        }
                    }
                }
            } else {
                removeFileFromDisk(context)
            }
        }
    }

    private fun removeFileFromDisk(context: Context) {
        scope.launch {
            synchronized(fileLock) {
                getFile(context)
                    .delete()
            }
        }
    }
}
