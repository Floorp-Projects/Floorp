/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration.session

import mozilla.components.browser.session.storage.RecoverableBrowserState
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.browser.state.state.recover.TabState
import mozilla.components.support.migration.Result
import org.json.JSONException
import org.json.JSONObject
import java.io.File
import java.io.IOException
import java.util.UUID

/**
 * Session store parser reading the whole JSON file into memory and then parsing it.
 */
internal object InMemorySessionStoreParser {
    @Throws(IOException::class, JSONException::class)
    internal fun parse(file: File): Result.Success<RecoverableBrowserState> {
        val json = file.readText()

        var selection = -1
        val sessions = mutableListOf<RecoverableTab>()

        val windows = JSONObject(json).getJSONArray("windows")
        if (windows.length() == 0) {
            return Result.Success(RecoverableBrowserState(emptyList(), null))
        }

        val window = windows.getJSONObject(0)
        val tabs = window.getJSONArray("tabs")
        val selectedTab = window.optInt("selected", -1)

        for (i in 0 until tabs.length()) {
            val tab = tabs.getJSONObject(i)

            val index = tab.getInt("index")
            val entries = tab.getJSONArray("entries")
            if (index < 1 || entries.length() < index) {
                // This tab has no entries at all. Let's just skip it.
                continue
            }

            val entry = entries.getJSONObject(index - 1)
            val url = entry.getString("url")
            val title = if (entry.isNull("title")) {
                url
            } else {
                entry.optString("title").ifEmpty { url }
            }

            val selected = selectedTab == i + 1

            sessions.add(
                RecoverableTab(
                    engineSessionState = null,
                    state = TabState(
                        id = UUID.randomUUID().toString(),
                        url = url,
                        title = title
                    )
                )
            )

            if (selected) {
                selection = sessions.size - 1
            }
        }

        return Result.Success(
            RecoverableBrowserState(
                sessions,
                sessions.getOrNull(selection)?.state?.id
            )
        )
    }
}
