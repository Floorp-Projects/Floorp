/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash

import android.os.Parcelable
import kotlinx.android.parcel.Parcelize
import java.util.Date

/**
 * Represents a single crash breadcrumb.
 */
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
    val date: Date = Date()
) : Parcelable, Comparable<Breadcrumb> {
    /**
     * Crash breadcrumb priority level.
     */
    enum class Level {
        /**
         * DEBUG level.
         */
        DEBUG,

        /**
         * INFO level.
         */
        INFO,

        /**
         * WARNING level.
         */
        WARNING,

        /**
         * ERROR level.
         */
        ERROR,

        /**
         * CRITICAL level.
         */
        CRITICAL
    }

    /**
     * Crash breadcrumb type.
     */
    enum class Type {
        /**
         * DEFAULT type.
         */
        DEFAULT,

        /**
         * HTTP type.
         */
        HTTP,

        /**
         * NAVIGATION type.
         */
        NAVIGATION,

        /**
         * USER type.
         */
        USER
    }

    override fun compareTo(other: Breadcrumb): Int {
        if (this.level.ordinal == other.level.ordinal) {
            return this.date.compareTo(other.date)
        }

        return this.level.ordinal.compareTo(other.level.ordinal)
    }
}
