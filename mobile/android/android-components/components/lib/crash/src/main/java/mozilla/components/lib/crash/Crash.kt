/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash

import android.content.Intent
import android.os.Bundle
import java.io.Serializable

// Intent extra used to store crash data under when passing crashes in Intent objects
private const val INTENT_CRASH = "mozilla.components.lib.crash.CRASH"

// Uncaught exception crash intent extras
private const val INTENT_EXCEPTION = "exception"

// Breadcrumbs intent extras
private const val INTENT_BREADCRUMBS = "breadcrumbs"

// Native code crash intent extras (Mirroring GeckoView values)
private const val INTENT_MINIDUMP_PATH = "minidumpPath"
private const val INTENT_EXTRAS_PATH = "extrasPath"
private const val INTENT_MINIDUMP_SUCCESS = "minidumpSuccess"
private const val INTENT_FATAL = "fatal"

/**
 * Crash types that are handled by this library.
 */
sealed class Crash {
    /**
     * A crash caused by an uncaught exception.
     *
     * @property throwable The [Throwable] that caused the crash.
     */
    data class UncaughtExceptionCrash(
        val throwable: Throwable,
        val breadcrumbs: ArrayList<Breadcrumb>
    ) : Crash() {
        override fun toBundle() = Bundle().apply {
            putSerializable(INTENT_EXCEPTION, throwable as Serializable)
            putParcelableArrayList(INTENT_BREADCRUMBS, breadcrumbs)
        }

        companion object {
            internal fun fromBundle(bundle: Bundle) = UncaughtExceptionCrash(
                bundle.getSerializable(INTENT_EXCEPTION) as Throwable,
                bundle.getParcelableArrayList(INTENT_BREADCRUMBS) ?: arrayListOf()
            )
        }
    }

    /**
     * A crash that happened in native code.
     *
     * @property minidumpPath Path to a Breakpad minidump file containing information about the crash.
     * @property minidumpSuccess Indicating whether or not the crash dump was successfully retrieved. If this is false,
     *                           the dump file may be corrupted or incomplete.
     * @property extrasPath Path to a file containing extra metadata about the crash. The file contains key-value pairs
     *                      in the form `Key=Value`. Be aware, it may contain sensitive data such as the URI that was
     *                      loaded at the time of the crash.
     * @property isFatal Whether or not the crash was fatal or not: If true, the main application process was affected
     *                   by the crash. If false, only an internal process used by Gecko has crashed and the application
     *                   may be able to recover.
     */
    data class NativeCodeCrash(
        val minidumpPath: String,
        val minidumpSuccess: Boolean,
        val extrasPath: String,
        val isFatal: Boolean,
        val breadcrumbs: ArrayList<Breadcrumb>
    ) : Crash() {
        override fun toBundle() = Bundle().apply {
            putString(INTENT_MINIDUMP_PATH, minidumpPath)
            putBoolean(INTENT_MINIDUMP_SUCCESS, minidumpSuccess)
            putString(INTENT_EXTRAS_PATH, extrasPath)
            putBoolean(INTENT_FATAL, isFatal)
            putParcelableArrayList(INTENT_BREADCRUMBS, breadcrumbs)
        }

        companion object {
            internal fun fromBundle(bundle: Bundle) = NativeCodeCrash(
                bundle.getString(INTENT_MINIDUMP_PATH, ""),
                bundle.getBoolean(INTENT_MINIDUMP_SUCCESS, false),
                bundle.getString(INTENT_EXTRAS_PATH, ""),
                bundle.getBoolean(INTENT_FATAL, false),
                bundle.getParcelableArrayList(INTENT_BREADCRUMBS) ?: arrayListOf()
            )
        }
    }

    internal abstract fun toBundle(): Bundle

    internal fun fillIn(intent: Intent) {
        intent.putExtra(INTENT_CRASH, toBundle())
    }

    companion object {
        fun fromIntent(intent: Intent): Crash {
            val bundle = intent.getBundleExtra(INTENT_CRASH)!!

            return if (bundle.containsKey(INTENT_MINIDUMP_PATH)) {
                NativeCodeCrash.fromBundle(bundle)
            } else {
                UncaughtExceptionCrash.fromBundle(bundle)
            }
        }

        fun isCrashIntent(intent: Intent) = intent.extras?.containsKey(INTENT_CRASH) ?: false
    }
}
