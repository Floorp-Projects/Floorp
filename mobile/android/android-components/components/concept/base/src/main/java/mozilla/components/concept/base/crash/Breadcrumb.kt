/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.base.crash

import android.annotation.SuppressLint
import android.os.Parcelable
import kotlinx.parcelize.Parcelize
import org.json.JSONObject
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.TimeZone

/**
 * Represents a single crash breadcrumb.
 */
@SuppressLint("ParcelCreator")
@Parcelize
data class Breadcrumb(
    /**
     * Message of the crash breadcrumb.
     */
    val message: String = "",

    /**
     * Data related to the crash breadcrumb.
     */
    val data: Map<String, String> = emptyMap(),

    /**
     * Category of the crash breadcrumb.
     */
    val category: String = "",

    /**
     * Level of the crash breadcrumb.
     */
    val level: Level = Level.DEBUG,

    /**
     * Type of the crash breadcrumb.
     */
    val type: Type = Type.DEFAULT,

    /**
     * Date of of the crash breadcrumb.
     */
    val date: Date = Date(),
) : Parcelable, Comparable<Breadcrumb> {
    /**
     * Crash breadcrumb priority level.
     */
    enum class Level(val value: String) {
        /**
         * DEBUG level.
         */
        DEBUG("Debug"),

        /**
         * INFO level.
         */
        INFO("Info"),

        /**
         * WARNING level.
         */
        WARNING("Warning"),

        /**
         * ERROR level.
         */
        ERROR("Error"),

        /**
         * CRITICAL level.
         */
        CRITICAL("Critical"),
    }

    /**
     * Crash breadcrumb type.
     */
    enum class Type(val value: String) {
        /**
         * DEFAULT type.
         */
        DEFAULT("Default"),

        /**
         * HTTP type.
         */
        HTTP("Http"),

        /**
         * NAVIGATION type.
         */
        NAVIGATION("Navigation"),

        /**
         * USER type.
         */
        USER("User"),
    }

    override fun compareTo(other: Breadcrumb): Int {
        return this.date.compareTo(other.date)
    }

    /**
     * Converts Breadcrumb into a JSON object
     *
     * @return A [JSONObject] that contains the information within the [Breadcrumb]
     */
    fun toJson(): JSONObject {
        val simpleDateFormat = SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss", Locale.US)
        simpleDateFormat.timeZone = TimeZone.getTimeZone("GMT")
        val jsonObject = JSONObject()
        jsonObject.put("timestamp", simpleDateFormat.format(this.date))
        jsonObject.put("message", this.message)
        jsonObject.put("category", this.category)
        jsonObject.put("level", this.level.value)
        jsonObject.put("type", this.type.value)

        val dataJsonObject = JSONObject()
        for ((k, v) in this.data) {
            dataJsonObject.put(k, v)
        }

        jsonObject.put("data", dataJsonObject)
        return jsonObject
    }
}
