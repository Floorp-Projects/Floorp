/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils.view

interface GroupableRadioButton {
    fun updateRadioValue(isChecked: Boolean)

    fun addToRadioGroup(radioButton: GroupableRadioButton)
}

/**
 * Connect all the given radio buttons into a group,
 * so that when one radio is checked the others are unchecked.
 */
fun addToRadioGroup(vararg radios: GroupableRadioButton) {
    for (i in 0..radios.lastIndex) {
        for (j in (i + 1)..radios.lastIndex) {
            radios[i].addToRadioGroup(radios[j])
            radios[j].addToRadioGroup(radios[i])
        }
    }
}

fun Iterable<GroupableRadioButton>.uncheckAll() {
    forEach { it.updateRadioValue(isChecked = false) }
}
