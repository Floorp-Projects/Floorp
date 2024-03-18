/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.test.fakes.android

import android.content.SharedPreferences

/**
 * A simple [SharedPreferences] implementation backed by an in-memory map. Helpful in unit tests and
 * faster than launching all Robolectric bells and whistles.
 */
@Suppress("UNCHECKED_CAST")
class FakeSharedPreferences(
    internal val values: MutableMap<String, Any> = mutableMapOf(),
) : SharedPreferences {
    override fun getAll(): Map<String, *> = values
    override fun getString(key: String, defValue: String?): String? = values[key]?.toString() ?: defValue
    override fun getStringSet(key: String, defValues: MutableSet<String>?): Set<String>? =
        values[key] as? Set<String> ?: defValues
    override fun getInt(key: String, defValue: Int): Int = values[key] as? Int ?: defValue
    override fun getLong(key: String, defValue: Long): Long = values[key] as? Long ?: defValue
    override fun getFloat(key: String, defValue: Float): Float = values[key] as? Float ?: defValue
    override fun getBoolean(key: String, defValue: Boolean): Boolean = values[key] as? Boolean ?: defValue
    override fun contains(key: String): Boolean = values.containsKey(key)
    override fun edit(): SharedPreferences.Editor = FakeEditor(this)
    override fun registerOnSharedPreferenceChangeListener(
        listener: SharedPreferences.OnSharedPreferenceChangeListener,
    ) = throw NotImplementedError()
    override fun unregisterOnSharedPreferenceChangeListener(
        listener: SharedPreferences.OnSharedPreferenceChangeListener,
    ) = throw NotImplementedError()
}

internal class FakeEditor(
    private val preferences: FakeSharedPreferences,
) : SharedPreferences.Editor {
    override fun putString(key: String, value: String?): SharedPreferences.Editor {
        if (value == null) {
            remove(key)
        } else {
            preferences.values[key] = value
        }
        return this
    }

    override fun putStringSet(key: String, values: MutableSet<String>?): SharedPreferences.Editor {
        if (values == null) {
            remove(key)
        } else {
            preferences.values[key] = values
        }
        return this
    }

    override fun putInt(key: String, value: Int): SharedPreferences.Editor {
        preferences.values[key] = value
        return this
    }

    override fun putLong(key: String, value: Long): SharedPreferences.Editor {
        preferences.values[key] = value
        return this
    }

    override fun putFloat(key: String, value: Float): SharedPreferences.Editor {
        preferences.values[key] = value
        return this
    }

    override fun putBoolean(key: String, value: Boolean): SharedPreferences.Editor {
        preferences.values[key] = value
        return this
    }

    override fun remove(key: String): SharedPreferences.Editor {
        preferences.values.remove(key)
        return this
    }

    override fun clear(): SharedPreferences.Editor {
        preferences.values.clear()
        return this
    }

    override fun commit(): Boolean = true

    override fun apply() = Unit
}
