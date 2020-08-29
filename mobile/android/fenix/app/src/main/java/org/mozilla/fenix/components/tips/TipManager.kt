/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.tips

import android.graphics.drawable.Drawable

sealed class TipType {
    data class Button(val text: String, val action: () -> Unit) : TipType()
}

open class Tip(
    val type: TipType,
    val identifier: String,
    val title: String,
    val description: String,
    val learnMoreURL: String?,
    val titleDrawable: Drawable? = null
)

interface TipProvider {
    val tip: Tip?
    val shouldDisplay: Boolean
}

interface TipManager {
    fun getTip(): Tip?
}

class FenixTipManager(
    private val providers: List<TipProvider>
) : TipManager {
    override fun getTip(): Tip? {
        return providers
            .firstOrNull { it.shouldDisplay }
            ?.tip
    }
}
