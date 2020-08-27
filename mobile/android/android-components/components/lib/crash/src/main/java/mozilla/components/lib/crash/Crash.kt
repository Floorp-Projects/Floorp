/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash

import android.content.Intent
import android.os.Bundle
import mozilla.components.support.base.crash.Breadcrumb
import java.io.Serializable
import java.util.UUID

// Intent extra used to store crash data under when passing crashes in Intent objects
private const val INTENT_CRASH = "mozilla.components.lib.crash.CRASH"

// Uncaught exception crash intent extras
private const val INTENT_EXCEPTION = "exception"

// Breadcrumbs intent extras
private const val INTENT_BREADCRUMBS = "breadcrumbs"

// Crash timestamp intent extras
private const val INTENT_CRASH_TIMESTAMP = "crashTimestamp"

// Native code crash intent extras (Mirroring GeckoView values)
private const val INTENT_UUID = "uuid"
private const val INTENT_MINIDUMP_PATH = "minidumpPath"
private const val INTENT_EXTRAS_PATH = "extrasPath"
private const val INTENT_MINIDUMP_SUCCESS = "minidumpSuccess"
private const val INTENT_FATAL = "fatal"

/**
 * Crash types that are handled by this library.
 */
sealed class Crash {
    /**
     * Unique ID identifying this crash.
     */
    abstract val uuid: String

    /**
     * A crash caused by an uncaught exception.
     *
     * @property timestamp Time of when the crash happened.
     * @property throwable The [Throwable] that caused the crash.
     * @property breadcrumbs List of breadcrumbs to send with the crash report.
     */
    data class UncaughtExceptionCrash(
        val timestamp: Long,
        val throwable: Throwable,
        val breadcrumbs: ArrayList<Breadcrumb>,
        override val uuid: String = UUID.randomUUID().toString()
    ) : Crash() {
        override fun toBundle() = Bundle().apply {
            putString(INTENT_UUID, uuid)
            putSerializable(INTENT_EXCEPTION, throwable as Serializable)
            putLong(INTENT_CRASH_TIMESTAMP, timestamp)
            putParcelableArrayList(INTENT_BREADCRUMBS, breadcrumbs)
        }

        companion object {
            internal fun fromBundle(bundle: Bundle) = UncaughtExceptionCrash(
                uuid = bundle.getString(INTENT_UUID) as String,
                throwable = bundle.getSerializable(INTENT_EXCEPTION) as Throwable,
                breadcrumbs = bundle.getParcelableArrayList(INTENT_BREADCRUMBS) ?: arrayListOf(),
                timestamp = bundle.getLong(INTENT_CRASH_TIMESTAMP, System.currentTimeMillis())
            )
        }
    }

    /**
     * A crash that happened in native code.
     *
     * @property timestamp Time of when the crash happened.
     * @property minidumpPath Path to a Breakpad minidump file containing information about the crash.
     * @property minidumpSuccess Indicating whether or not the crash dump was successfully retrieved. If this is false,
     *                           the dump file may be corrupted or incomplete.
     * @property extrasPath Path to a file containing extra metadata about the crash. The file contains key-value pairs
     *                      in the form `Key=Value`. Be aware, it may contain sensitive data such as the URI that was
     *                      loaded at the time of the crash.
     * @property isFatal Whether or not the crash was fatal or not: If true, the main application process was affected
     *                   by the crash. If false, only an internal process used by Gecko has crashed and the application
     *                   may be able to recover.
     * @property breadcrumbs List of breadcrumbs to send with the crash report.
     */
    data class NativeCodeCrash(
        val timestamp: Long,
        val minidumpPath: String?,
        val minidumpSuccess: Boolean,
        val extrasPath: String?,
        val isFatal: Boolean,
        val breadcrumbs: ArrayList<Breadcrumb>,
        override val uuid: String = UUID.randomUUID().toString()
    ) : Crash() {
        override fun toBundle() = Bundle().apply {
            putString(INTENT_UUID, uuid)
            putString(INTENT_MINIDUMP_PATH, minidumpPath)
            putBoolean(INTENT_MINIDUMP_SUCCESS, minidumpSuccess)
            putString(INTENT_EXTRAS_PATH, extrasPath)
            putBoolean(INTENT_FATAL, isFatal)
            putLong(INTENT_CRASH_TIMESTAMP, timestamp)
            putParcelableArrayList(INTENT_BREADCRUMBS, breadcrumbs)
        }

        companion object {
            internal fun fromBundle(bundle: Bundle) = NativeCodeCrash(
                uuid = bundle.getString(INTENT_UUID) ?: UUID.randomUUID().toString(),
                minidumpPath = bundle.getString(INTENT_MINIDUMP_PATH, null),
                minidumpSuccess = bundle.getBoolean(INTENT_MINIDUMP_SUCCESS, false),
                extrasPath = bundle.getString(INTENT_EXTRAS_PATH, null),
                isFatal = bundle.getBoolean(INTENT_FATAL, false),
                breadcrumbs = bundle.getParcelableArrayList(INTENT_BREADCRUMBS) ?: arrayListOf(),
                timestamp = bundle.getLong(INTENT_CRASH_TIMESTAMP, System.currentTimeMillis())
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
