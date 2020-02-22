/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.contextmenu.ext

import android.content.Context
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.contextmenu.DefaultSelectionActionDelegate
import mozilla.components.feature.search.BrowserStoreSearchAdapter

/**
 * More convenient secondary constructor for creating a [DefaultSelectionActionDelegate].
 */
@Suppress("FunctionName")
fun DefaultSelectionActionDelegate(
    store: BrowserStore,
    context: Context,
    appName: String,
    shareTextClicked: ((String) -> Unit)? = null
) =
    DefaultSelectionActionDelegate(
        BrowserStoreSearchAdapter(store),
        context.resources,
        appName,
        shareTextClicked
    )
