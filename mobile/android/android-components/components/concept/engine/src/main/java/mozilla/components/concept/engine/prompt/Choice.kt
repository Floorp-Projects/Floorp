/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.prompt

import android.os.Parcel
import android.os.Parcelable

/**
 * Value type that represents a select option, optgroup or menuitem html element.
 *
 * @property id of the option, optgroup or menuitem.
 * @property enable indicate if item should be selectable or not.
 * @property label The label for displaying the option, optgroup or menuitem.
 * @property selected Indicate if the item should be pre-selected.
 * @property isASeparator Indicating if the item should be a menu separator (only valid for menus).
 * @property children Sub-items in a group, or null if not a group.
 */
data class Choice(
    val id: String,
    var enable: Boolean = true,
    var label: String,
    var selected: Boolean = false,
    val isASeparator:
        Boolean = false,
    val children: Array<Choice>? = null,
) : Parcelable {

    val isGroupType get() = children != null

    internal constructor(parcel: Parcel) : this(
        parcel.readString() ?: "",
        parcel.readByte() != 0.toByte(),
        parcel.readString() ?: "",
        parcel.readByte() != 0.toByte(),
        parcel.readByte() != 0.toByte(),
        parcel.createTypedArray(CREATOR),
    )

    override fun writeToParcel(parcel: Parcel, flags: Int) {
        parcel.writeString(id)
        parcel.writeByte(if (enable) 1 else 0)
        parcel.writeString(label)
        parcel.writeByte(if (selected) 1 else 0)
        parcel.writeByte(if (isASeparator) 1 else 0)
        parcel.writeTypedArray(children, flags)
    }

    override fun describeContents(): Int {
        return 0
    }

    companion object CREATOR : Parcelable.Creator<Choice> {
        override fun createFromParcel(parcel: Parcel): Choice {
            return Choice(parcel)
        }

        override fun newArray(size: Int): Array<Choice?> {
            return arrayOfNulls(size)
        }
    }
}
