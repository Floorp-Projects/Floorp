/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.readerview.internal

import android.content.Context
import android.content.Context.MODE_PRIVATE
import android.content.res.Configuration
import mozilla.components.feature.readerview.ReaderViewFeature
import mozilla.components.feature.readerview.ReaderViewFeature.Companion.ACTION_CHANGE_FONT_SIZE
import mozilla.components.feature.readerview.ReaderViewFeature.Companion.ACTION_MESSAGE_KEY
import mozilla.components.feature.readerview.ReaderViewFeature.Companion.ACTION_SET_FONT_TYPE
import mozilla.components.feature.readerview.ReaderViewFeature.Companion.ACTION_VALUE
import mozilla.components.feature.readerview.ReaderViewFeature.Companion.COLOR_SCHEME_KEY
import mozilla.components.feature.readerview.ReaderViewFeature.Companion.FONT_SIZE_DEFAULT
import mozilla.components.feature.readerview.ReaderViewFeature.Companion.FONT_SIZE_KEY
import mozilla.components.feature.readerview.ReaderViewFeature.Companion.FONT_TYPE_KEY
import mozilla.components.feature.readerview.ReaderViewFeature.Companion.SHARED_PREF_NAME
import org.json.JSONObject

/**
 * Stores the user configuration for reader view in shared prefs.
 * All values are initialized lazily and cached.
 * @param context Used to lazily obtain shared preferences and to check dark mode status.
 * @param sendConfigMessage If the config changes, this method will be invoked
 * with a JSON object which should be sent to the content script so the new
 * config can be applied.
 */
internal class ReaderViewConfig(
    context: Context,
    private val sendConfigMessage: (JSONObject) -> Unit,
) {

    private val prefs by lazy { context.getSharedPreferences(SHARED_PREF_NAME, MODE_PRIVATE) }
    private val resources = context.resources
    private var colorSchemeCache: ReaderViewFeature.ColorScheme? = null
    private var fontTypeCache: ReaderViewFeature.FontType? = null
    private var fontSizeCache: Int? = null

    var colorScheme: ReaderViewFeature.ColorScheme
        get() {
            if (colorSchemeCache == null) {
                // Default to a dark theme if either the system or local dark theme is active
                val defaultColor = if (isNightMode()) {
                    ReaderViewFeature.ColorScheme.DARK
                } else {
                    ReaderViewFeature.ColorScheme.LIGHT
                }
                colorSchemeCache = getEnumFromPrefs(COLOR_SCHEME_KEY, defaultColor)
            }
            return colorSchemeCache!!
        }
        set(value) {
            if (colorSchemeCache != value) {
                colorSchemeCache = value
                prefs.edit().putString(COLOR_SCHEME_KEY, value.name).apply()
                sendMessage(ReaderViewFeature.ACTION_SET_COLOR_SCHEME) { put(ACTION_VALUE, value.name) }
            }
        }

    var fontType: ReaderViewFeature.FontType
        get() {
            if (fontTypeCache == null) {
                fontTypeCache = getEnumFromPrefs(FONT_TYPE_KEY, ReaderViewFeature.FontType.SERIF)
            }
            return fontTypeCache!!
        }
        set(value) {
            if (fontTypeCache != value) {
                fontTypeCache = value
                prefs.edit().putString(FONT_TYPE_KEY, value.name).apply()
                sendMessage(ACTION_SET_FONT_TYPE) { put(ACTION_VALUE, value.value) }
            }
        }

    var fontSize: Int
        get() {
            if (fontSizeCache == null) {
                fontSizeCache = prefs.getInt(FONT_SIZE_KEY, FONT_SIZE_DEFAULT)
            }
            return fontSizeCache!!
        }
        set(value) {
            if (fontSizeCache != value) {
                val diff = value - fontSize
                fontSizeCache = value
                prefs.edit().putInt(FONT_SIZE_KEY, value).apply()
                sendMessage(ACTION_CHANGE_FONT_SIZE) { put(ACTION_VALUE, diff) }
            }
        }

    private inline fun <reified T : Enum<T>> getEnumFromPrefs(key: String, default: T): T {
        val enumName = prefs.getString(key, default.name) ?: default.name
        return enumValueOf(enumName)
    }

    private fun isNightMode(): Boolean {
        val darkFlag = resources?.configuration?.uiMode?.and(Configuration.UI_MODE_NIGHT_MASK)
        return darkFlag == Configuration.UI_MODE_NIGHT_YES
    }

    private inline fun sendMessage(action: String, crossinline builder: JSONObject.() -> Unit) {
        val message = JSONObject().put(ACTION_MESSAGE_KEY, action)
        builder(message)
        sendConfigMessage(message)
    }
}
