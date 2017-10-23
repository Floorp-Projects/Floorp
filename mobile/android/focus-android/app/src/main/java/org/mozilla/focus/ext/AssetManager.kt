/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.ext

import android.content.res.AssetManager
import edu.umd.cs.findbugs.annotations.SuppressFBWarnings
import org.json.JSONObject

// Extension functions for the AssetManager class

@SuppressFBWarnings(
        value = "SA_LOCAL_SELF_ASSIGNMENT",
        justification = "The Kotlin 1.1 compiler generates bytecode that contains unnecessary self assignments. This should be fixed in 1.2.")
fun AssetManager.readJSONObject(fileName: String) = JSONObject(open(fileName).bufferedReader().use {
    it.readText()
})
