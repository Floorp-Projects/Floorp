/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.preparer

import android.content.Context
import mozilla.components.browser.icons.IconRequest

/**
 * An [IconPreparer] implementation receives an [IconRequest] before it is getting loaded. The preparer has the option
 * to rewrite the [IconRequest] and return a new instance.
 */
interface IconPreprarer {
    fun prepare(context: Context, request: IconRequest): IconRequest
}
