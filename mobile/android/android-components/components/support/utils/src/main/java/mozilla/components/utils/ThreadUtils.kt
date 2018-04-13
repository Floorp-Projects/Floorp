/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.utils

import android.os.Handler
import android.os.Looper

import java.util.concurrent.Executors

object ThreadUtils {
    // We lazily instantiate this thread so it's only taking up resources if it's used by the project.
    private val backgroundExecutorService by lazy { Executors.newSingleThreadExecutor() }
    private val handler = Handler(Looper.getMainLooper())
    private val uiThread = Looper.getMainLooper().thread

    fun postToBackgroundThread(runnable: Runnable) {
        backgroundExecutorService.submit(runnable)
    }

    fun postToMainThread(runnable: Runnable) {
        handler.post(runnable)
    }

    fun postToMainThreadDelayed(runnable: Runnable, delayMillis: Long) {
        handler.postDelayed(runnable, delayMillis)
    }

    fun assertOnUiThread() {
        val currentThread = Thread.currentThread()
        val currentThreadId = currentThread.id
        val expectedThreadId = uiThread.id

        if (currentThreadId == expectedThreadId) {
            return
        }

        throw IllegalThreadStateException("Expected UI thread, but running on " + currentThread.name)
    }
}
