/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.utils

import android.content.Context
import android.content.SharedPreferences
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONException
import org.json.JSONObject

/**
 * An abstract wrapper around [SharedPreferences] which facilitates caching of [T] objects.
 */
abstract class SharedPreferencesCache<T>(val context: Context) {
    /**
     * Logger used to report issues.
     */
    abstract val logger: Logger

    /**
     * Name of the 'key' under which serialized data is stored within the cache.
     */
    abstract val cacheKey: String

    /**
     * Name of the cache.
     */
    abstract val cacheName: String

    /**
     * A conversion method from [T] into a [JSONObject].
     */
    abstract fun T.toJSON(): JSONObject

    /**
     * A conversion method from [JSONObject] to [T].
     */
    abstract fun fromJSON(obj: JSONObject): T

    /**
     * @param A [T] value to cache.
     */
    fun setToCache(obj: T) {
        // JSONObject swallows any 'JSONException' thrown in 'toString', and simply returns 'null'.
        // An error happened while converting a JSONObject into a string. Let's fail loudly and
        // see if this actually happens in the wild. An alternative is to swallow this error and
        // log an error message, but we're very unlikely to notice any problems in that case.
        val s = obj.toJSON().toString() as String? ?: throw IllegalStateException("Failed to stringify")
        cache().edit().putString(cacheKey, s).apply()
    }

    /**
     * @return Cached [T] value or `null`.
     */
    fun getCached(): T? {
        val s = cache().getString(cacheKey, null) ?: return null
        return try {
            fromJSON(JSONObject(s))
        } catch (e: JSONException) {
            logger.error("Failed to parse cached value", e)
            null
        }
    }

    /**
     * Clear cached values.
     */
    fun clear() {
        cache().edit().clear().apply()
    }

    private fun cache(): SharedPreferences {
        return context.getSharedPreferences(cacheName, Context.MODE_PRIVATE)
    }
}
