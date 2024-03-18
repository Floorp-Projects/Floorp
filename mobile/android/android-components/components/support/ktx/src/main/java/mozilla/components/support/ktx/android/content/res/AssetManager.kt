/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.content.res

import android.content.res.AssetManager
import org.json.JSONObject

/**
 * Read a file from the "assets" and create a a JSONObject from its content.
 *
 * @param fileName The name of the asset to open.  This name can be
 *                 hierarchical.
 */
fun AssetManager.readJSONObject(fileName: String) = JSONObject(
    open(fileName).bufferedReader().use {
        it.readText()
    },
)
