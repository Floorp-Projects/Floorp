/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.ids

import android.content.Context
import android.content.SharedPreferences

private const val KEY_NEXT_ID = "nextId"
private const val KEY_LAST_USED_PREFIX = "lastUsed."
private const val KEY_ID_PREFIX = "id."

/**
 * Internal helper to create unique and stable [Int] ids based on [String] tags.
 *
 * @param fileName The shared preference file that should be used to save id assignments.
 * @param idLifeTime The maximum time an id can be unused until it is cleared
 * @param offset The [Int] offset from which this instance should start providing ids.
 */
internal class SharedIds(
    private val fileName: String,
    private val idLifeTime: Long,
    private val offset: Int = 0,
) {
    /**
     * Get a unique ID for the provided unique tag.
     */
    @Synchronized
    fun getIdForTag(context: Context, tag: String): Int {
        val preferences = preferences(context)
        val editor = preferences.edit()
        val key = tagToKey(tag)
        val lastUsedKey = tagToLastUsedKey(tag)

        removeExpiredIds(preferences, editor)

        // First check if we already have an id for this tag
        val id = preferences.getInt(key, -1)
        if (id != -1) {
            editor.putLong(lastUsedKey, now()).apply()
            return id
        }

        // If we do not have an id for this tag then use the next available one and save that.
        val nextId = preferences.getInt(KEY_NEXT_ID, offset)

        editor.putInt(KEY_NEXT_ID, nextId + 1) // Ignoring overflow for now.
        editor.putInt(key, nextId)
        editor.putLong(lastUsedKey, now())
        editor.apply()

        return nextId
    }

    /**
     * Get the next available unique ID for the provided unique tag.
     */
    @Synchronized
    fun getNextIdForTag(context: Context, tag: String): Int {
        val preferences = preferences(context)
        val editor = preferences.edit()
        val key = tagToKey(tag)
        val lastUsedKey = tagToLastUsedKey(tag)

        removeExpiredIds(preferences, editor)

        // always use the next available one and save that.
        val nextId = preferences.getInt(KEY_NEXT_ID, offset)

        editor.putInt(KEY_NEXT_ID, nextId + 1) // Ignoring overflow for now.
        editor.putInt(key, nextId)
        editor.putLong(lastUsedKey, now())
        editor.apply()

        return nextId
    }

    private fun tagToKey(tag: String): String {
        return "$KEY_ID_PREFIX.$tag"
    }

    private fun tagToLastUsedKey(tag: String): String {
        return "$KEY_LAST_USED_PREFIX$tag"
    }

    private fun preferences(context: Context): SharedPreferences {
        return context.getSharedPreferences(fileName, Context.MODE_PRIVATE)
    }

    /**
     * Remove all expired notification ids.
     */
    private fun removeExpiredIds(preferences: SharedPreferences, editor: SharedPreferences.Editor) {
        preferences.all.entries.filter { entry ->
            entry.key.startsWith(KEY_LAST_USED_PREFIX)
        }.filter { entry ->
            val lastUsed = entry.value as Long
            lastUsed < now() - idLifeTime
        }.forEach { entry ->
            val lastUsedKey = entry.key
            val tag = lastUsedKey.substring(KEY_LAST_USED_PREFIX.length)
            editor.remove(tagToKey(tag))
            editor.remove(lastUsedKey)
            editor.apply()
        }
    }

    fun clear(context: Context) { preferences(context).edit().clear().apply() }

    internal var now: () -> Long = { System.currentTimeMillis() }
}
