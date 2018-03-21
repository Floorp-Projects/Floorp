/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.utils

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.os.Parcelable
import android.util.Log

import java.util.ArrayList

/**
 * External applications can pass values into Intents that can cause us to crash: in defense,
 * we wrap [Intent] and catch the exceptions they may force us to throw. See bug 1090385
 * for more.
 */
class SafeIntent(val unsafe: Intent) {

    val extras: Bundle?
        get() {
            try {
                return unsafe.extras
            } catch (e: OutOfMemoryError) {
                Log.w(LOGTAG, "Couldn't get intent extras: OOM. Malformed?")
                return null
            } catch (e: RuntimeException) {
                Log.w(LOGTAG, "Couldn't get intent extras.", e)
                return null
            }
        }

    val action: String?
        get() = unsafe.action

    val flags: Int
        get() = unsafe.flags

    val dataString: String?
        get() {
            try {
                return unsafe.dataString
            } catch (e: OutOfMemoryError) {
                Log.w(LOGTAG, "Couldn't get intent data string: OOM. Malformed?")
                return null
            } catch (e: RuntimeException) {
                Log.w(LOGTAG, "Couldn't get intent data string.", e)
                return null
            }
        }

    val data: Uri?
        get() {
            try {
                return unsafe.data
            } catch (e: OutOfMemoryError) {
                Log.w(LOGTAG, "Couldn't get intent data: OOM. Malformed?")
                return null
            } catch (e: RuntimeException) {
                Log.w(LOGTAG, "Couldn't get intent data.", e)
                return null
            }
        }

    val categories: Set<String>?
        get() {
            try {
                return unsafe.categories
            } catch (e: OutOfMemoryError) {
                Log.w(LOGTAG, "Couldn't get intent categories: OOM. Malformed?")
                return null
            } catch (e: RuntimeException) {
                Log.w(LOGTAG, "Couldn't get intent categories.", e)
                return null
            }
        }

    val isLauncherIntent: Boolean
        get() {
            val intentCategories = unsafe.categories
            return intentCategories != null && intentCategories.contains(Intent.CATEGORY_LAUNCHER) && Intent.ACTION_MAIN == unsafe.action
        }

    fun hasExtra(name: String): Boolean {
        try {
            return unsafe.hasExtra(name)
        } catch (e: OutOfMemoryError) {
            Log.w(LOGTAG, "Couldn't determine if intent had an extra: OOM. Malformed?")
            return false
        } catch (e: RuntimeException) {
            Log.w(LOGTAG, "Couldn't determine if intent had an extra.", e)
            return false
        }
    }

    fun getBooleanExtra(name: String, defaultValue: Boolean): Boolean {
        try {
            return unsafe.getBooleanExtra(name, defaultValue)
        } catch (e: OutOfMemoryError) {
            Log.w(LOGTAG, "Couldn't get intent extras: OOM. Malformed?")
            return defaultValue
        } catch (e: RuntimeException) {
            Log.w(LOGTAG, "Couldn't get intent extras.", e)
            return defaultValue
        }
    }

    fun getIntExtra(name: String, defaultValue: Int): Int {
        try {
            return unsafe.getIntExtra(name, defaultValue)
        } catch (e: OutOfMemoryError) {
            Log.w(LOGTAG, "Couldn't get intent extras: OOM. Malformed?")
            return defaultValue
        } catch (e: RuntimeException) {
            Log.w(LOGTAG, "Couldn't get intent extras.", e)
            return defaultValue
        }
    }

    fun getStringExtra(name: String): String? {
        try {
            return unsafe.getStringExtra(name)
        } catch (e: OutOfMemoryError) {
            Log.w(LOGTAG, "Couldn't get intent extras: OOM. Malformed?")
            return null
        } catch (e: RuntimeException) {
            Log.w(LOGTAG, "Couldn't get intent extras.", e)
            return null
        }
    }

    fun getBundleExtra(name: String): SafeBundle? {
        try {
            val bundle = unsafe.getBundleExtra(name)
            return if (bundle != null) {
                SafeBundle(bundle)
            } else {
                null
            }
        } catch (e: OutOfMemoryError) {
            Log.w(LOGTAG, "Couldn't get intent extras: OOM. Malformed?")
            return null
        } catch (e: RuntimeException) {
            Log.w(LOGTAG, "Couldn't get intent extras.", e)
            return null
        }
    }

    fun getCharSequenceExtra(name: String): CharSequence? {
        try {
            return unsafe.getCharSequenceExtra(name)
        } catch (e: OutOfMemoryError) {
            Log.w(LOGTAG, "Couldn't get intent extras: OOM. Malformed?")
            return null
        } catch (e: RuntimeException) {
            Log.w(LOGTAG, "Couldn't get intent extras.", e)
            return null
        }
    }

    fun <T : Parcelable> getParcelableExtra(name: String): T? {
        try {
            return unsafe.getParcelableExtra(name)
        } catch (e: OutOfMemoryError) {
            Log.w(LOGTAG, "Couldn't get intent extras: OOM. Malformed?")
            return null
        } catch (e: RuntimeException) {
            Log.w(LOGTAG, "Couldn't get intent extras.", e)
            return null
        }
    }

    fun <T : Parcelable> getParcelableArrayListExtra(name: String): ArrayList<T>? {
        try {
            return unsafe.getParcelableArrayListExtra(name)
        } catch (e: OutOfMemoryError) {
            Log.w(LOGTAG, "Couldn't get intent extras: OOM. Malformed?")
            return null
        } catch (e: RuntimeException) {
            Log.w(LOGTAG, "Couldn't get intent extras.", e)
            return null
        }
    }

    fun getStringArrayListExtra(name: String): ArrayList<String>? {
        try {
            return unsafe.getStringArrayListExtra(name)
        } catch (e: OutOfMemoryError) {
            Log.w(LOGTAG, "Couldn't get intent data string: OOM. Malformed?")
            return null
        } catch (e: RuntimeException) {
            Log.w(LOGTAG, "Couldn't get intent data string.", e)
            return null
        }
    }

    companion object {
        private val LOGTAG = "Gecko" + SafeIntent::class.java.simpleName
    }
}
